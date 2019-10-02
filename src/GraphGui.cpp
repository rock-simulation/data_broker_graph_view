/**
 * \file GraphGui.cpp
 * \author Malte Langosz (malte.langosz@dfki.de)
 *
 * Version 0.1
 */

#include "GraphGui.hpp"
#include <mars/utils/misc.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <osgViewer/View>
#include <osgGA/TrackballManipulator>
#include <osgQt/GraphicsWindowQt>
#include <configmaps/ConfigVector.hpp>
#include <configmaps/ConfigAtom.hpp>

using namespace configmaps;
using namespace mars::data_broker;

namespace data_broker_graph_view {

  GraphGui::GraphGui (mars::cfg_manager::CFGManagerInterface *cfg,
                      DataBrokerInterface *dataBroker)
    : mars::main_gui::BaseWidget(0, cfg, "DataBrokerGraph"),
      dataBroker(dataBroker) {
    resize (500, 300);
    setWindowTitle ("DataBrokerGraph");
    ignoreRemoveEdge = false;
  
    view = new osg_graph_viz::View();
    view->setResourcesPath(cfg->getOrCreateProperty("Preferences",
                                                    "resources_path",
                                                    ".").sValue+
                           "/osg_graph_viz/");
    view->init(16, 16, 1.5);
    view->setLineMode(osg_graph_viz::SMOOTH_LINE_MODE);
    view->setUpdateInterface(this);
    mars::cfg_manager::cfgPropertyStruct path;
    path = cfg->getOrCreateProperty("Config", "config_path", std::string("."));
    configPath = path.sValue;
    QWidget *viz;
    {
      osgQt::GLWidget *widget1 = NULL;
      viewer = new osgViewer::CompositeViewer();
      viewer->setThreadingModel(osgViewer::CompositeViewer::SingleThreaded);
      osgViewer::View *osgView = new osgViewer::View();
      osg::Camera *camera = new osg::Camera();
      osg::DisplaySettings* ds = osg::DisplaySettings::instance();
      osg::ref_ptr<osg::GraphicsContext::Traits> traits;
      traits = new osg::GraphicsContext::Traits;
      double w = 1920;
      double h = 1080;
      traits->readDISPLAY();
      if (traits->displayNum < 0)
        traits->displayNum = 0;
      traits->x = 100;
      traits->y = 100;
      traits->width = w;
      traits->height = h;
      traits->windowDecoration = true;
      traits->doubleBuffer = true;
      traits->alpha = ds->getMinimumNumAlphaBits();
      traits->stencil = ds->getMinimumNumStencilBits();
      traits->sampleBuffers = ds->getMultiSamples();
      traits->samples = ds->getNumMultiSamples();
      traits->vsync = false;
      traits->sharedContext = 0;
      osgQt::GraphicsWindowQt *gw2 = new osgQt::GraphicsWindowQt(traits.get());
      camera->setGraphicsContext(gw2);
      camera->setViewport( new ::osg::Viewport(0, 0, w, h) );
      camera->setProjectionMatrix(osg::Matrix::ortho2D(0,w,0,h));
      camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
      camera->setViewMatrix(osg::Matrix::identity());
      //camera->setClearMask(GL_DEPTH_BUFFER_BIT);
      camera->setRenderOrder(osg::Camera::POST_RENDER, 10);
      camera->setClearColor(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
      osg::StateSet *s = camera->getOrCreateStateSet();
      s->setMode(GL_LIGHTING, (osg::StateAttribute::OFF |
                               osg::StateAttribute::OVERRIDE |
                               osg::StateAttribute::PROTECTED));
      s->setMode(GL_DEPTH_TEST, (osg::StateAttribute::OFF |
                                 osg::StateAttribute::OVERRIDE |
                                 osg::StateAttribute::PROTECTED));
      s->setRenderingHint(osg::StateSet::TRANSPARENT_BIN |
                          osg::StateAttribute::OVERRIDE |
                          osg::StateAttribute::PROTECTED);
      s->setMode(GL_BLEND,osg::StateAttribute::ON |
                 osg::StateAttribute::OVERRIDE |
                 osg::StateAttribute::PROTECTED);
      view->setCamera(camera);
      osgView->setCamera(camera);
      osg::Group *g = new osg::Group();
      g->getOrCreateStateSet()->setGlobalDefaults();
      g->addChild(view->getScene());
      osgView->setSceneData(g);
      viewer->addView(osgView);
      widget1 = gw2 ? gw2->getGLWidget() : NULL;
      widget1->setGeometry(100, 100, 1920, 1080);
      viz = dynamic_cast<QWidget*>(widget1);
      // viewer->realize();
      osgView->addEventHandler(view.get());
      //views.front()->setCameraManipulator( new osgGA::TrackballManipulator );
      startTimer(10);      
    }

    viz->setMinimumWidth(200);
    viz->setMinimumHeight(200);
    QWidget *w = new QWidget(this);
    QVBoxLayout *vLayout = new QVBoxLayout();
    filterPatterns = new QTextEdit();
    QLabel *l = new QLabel("filter patterns:");
    vLayout->addWidget(l);
    vLayout->addWidget(filterPatterns);
    connect(filterPatterns, SIGNAL(textChanged()),
            this, SLOT(updatePackages()));
    QPushButton *button = new QPushButton("Save Connections");
    connect(button, SIGNAL(clicked()), this, SLOT(save()));
    vLayout->addWidget(button);

    w->setLayout(vLayout);
    QSplitter *splitter = new QSplitter();
    splitter->addWidget(viz);
    splitter->addWidget(w);
    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->addWidget(splitter);
    this->setLayout(hLayout);
    view->resize(viz->width(), viz->height());
    load();
    updatePackages();
    dataBroker->registerSyncReceiver(this, "data_broker", "newStream", 1);
  }

  GraphGui::~GraphGui () {
  }

  void GraphGui::timerEvent(QTimerEvent *event) {
    connectionMutex.lock();
    {
      std::vector<ConfigMap>::iterator it = newConnections.begin();
      for(; it!=newConnections.end(); ++it) {
        createConnection(*it);
      }
      newConnections.clear();
    }
    {
      std::vector<DataInfo>::iterator it = newNodes.begin();
      for(; it!=newNodes.end(); ++it) {
        addNode(it->groupName+":"+it->dataName, it->dataId, it->flags);
      }
      newNodes.clear();
    }
    connectionMutex.unlock();
    viewer->frame();
  }

  void GraphGui::updatePackages() {
    std::vector<DataInfo>::const_iterator it;
    const std::vector<DataInfo> &dataList = dataBroker->getDataList();
    ConfigMap map;
    double x, y;
    std::string name;
    ignoreRemoveEdge = true;
    for(size_t i=0; i<nodeList.size(); ++i) {
      // store the old node positions
      name = nodeList[i]->getName();
      map["name"] = name;
      nodeList[i]->getPosition(&x, &y);
      map["pos/x"] = x;
      map["pos/y"] = y;
      nodeConfigMap[name] = map;
      view->removeNode(nodeList[i]);
    }
    ignoreRemoveEdge = false;
    
    nodeList.clear();
    edgeList.clear();
    fprintf(stderr, "update packages: %lu\n", dataList.size());
    for(it=dataList.begin(); it!=dataList.end(); ++it) {
      addNode(it->groupName+":"+it->dataName, it->dataId, it->flags);
    }
    { /* create edges */
      std::vector<ConfigMap>::iterator it=edgeConfigList.begin();
      for(; it!=edgeConfigList.end(); ++it) {
        osg_graph_viz::Node *fromNode = 0, *toNode=0;
        // find fromNode
        std::vector<osg::ref_ptr<osg_graph_viz::Node> >::iterator jt = nodeList.begin();
        for(; jt!=nodeList.end(); ++jt) {
          if((*jt)->getName() == (std::string)(*it)["fromNode"]) {
            fromNode = *jt;
          }
          else if((*jt)->getName() == (std::string)(*it)["toNode"]) {
            toNode = *jt;
          }
        }
        if(!fromNode || !toNode) {
          continue;
        }
        // find ports
        int outIdx = -1;
        for(; outIdx+1<fromNode->getNodeInfo().numOutputs; ++outIdx) {
          if(fromNode->getOutPortName(outIdx+1) == (std::string)(*it)["fromNodeOutput"]) {outIdx++; break;}
        }
        int inIdx = -1;
        for(; inIdx+1<toNode->getNodeInfo().numInputs; ++inIdx) {
          if(toNode->getInPortName(inIdx+1) == (std::string)(*it)["toNodeInput"]) {inIdx++; break;}
        }
        fprintf(stderr, "update: %d %d\n", outIdx, inIdx);
        if(outIdx < 0 || outIdx >= fromNode->getNodeInfo().numOutputs) continue;
        if(inIdx < 0 || inIdx >= toNode->getNodeInfo().numInputs) continue;
        osg_graph_viz::Edge *edge = view->createEdge(*it, outIdx, inIdx);
        fromNode->addOutputEdge(outIdx, edge);
        toNode->addInputEdge(inIdx, edge);
        fromNode->updateEdges();
        toNode->updateEdges();
        edgeList.push_back(edge);
      }
    }
  }

  void GraphGui::newEdge(osg_graph_viz::Edge *edge,
                         osg_graph_viz::Node* fromNode,
                         int fromNodeIdx,
                         osg_graph_viz::Node* toNode, int toNodeIdx) {
    ConfigMap edgeConfig = edge->getMap();
    ConfigMap node = fromNode->getMap();
    edgeConfig["fromNode"] = node["name"];
    edgeConfig["fromNodeOutput"] = node["outputs"][fromNodeIdx]["name"];
    node = toNode->getMap();
    edgeConfig["toNode"] = node["name"];
    edgeConfig["toNodeInput"] = node["inputs"][toNodeIdx]["name"];
    edgeConfig["weight"] = 1.0;
    edge->updateMap(edgeConfig);
    edgeList.push_back(edge);
    createDataBrokerConnection(edgeConfig);
  }
      
  bool GraphGui::removeEdge(osg_graph_viz::Edge* edge) {
    if(ignoreRemoveEdge) return true;
    std::vector<osg::ref_ptr<osg_graph_viz::Edge> >::iterator it;
    for(it=edgeList.begin(); it!=edgeList.end(); ++it) {
      if(*it == edge) {
        ConfigMap edgeConfig = edge->getMap();
        {
          std::string fromGroup, fromPackage, fromItem, toGroup, toPackage, toItem;
          std::vector<std::string> s;
          s = mars::utils::explodeString(':', edgeConfig["fromNode"]);
          fromGroup = s[0];
          fromPackage = "";
          for(size_t i=1; i<s.size(); ++i) {
            fromPackage.append(s[i]);
          }
          fromItem << edgeConfig["fromNodeOutput"];

          s = mars::utils::explodeString(':', edgeConfig["toNode"]);
          toGroup = s[0];
          toPackage = "";
          for(size_t i=1; i<s.size(); ++i) {
            toPackage.append(s[i]);
          }
          toItem << edgeConfig["toNodeInput"];
          fprintf(stderr, "disconnect: %s:%s:%s -> %s:%s:%s\n", fromGroup.c_str(),
                  fromPackage.c_str(), fromItem.c_str(), toGroup.c_str(),
                  toPackage.c_str(), toItem.c_str());
          dataBroker->disconnectDataItems(fromGroup, fromPackage, fromItem,
                                          toGroup, toPackage, toItem);
        }
        { /* remove edge from edgeConfigList */
          std::vector<ConfigMap>::iterator it = edgeConfigList.begin();
          for(; it!=edgeConfigList.end(); ++it) {
            if((std::string)edgeConfig["fromNode"] != (std::string)(*it)["fromNode"]) continue;
            if((std::string)edgeConfig["fromNodeOutput"] != (std::string)(*it)["fromNodeOutput"]) continue;
            if((std::string)edgeConfig["toNode"] != (std::string)(*it)["toNode"]) continue;
            if((std::string)edgeConfig["toNodeInput"] != (std::string)(*it)["toNodeInput"]) continue;
            edgeConfigList.erase(it);
            break;
          }
        }
        edgeList.erase(it);
        break;
      }
    }
    return true;
  }

  void GraphGui::createDataBrokerConnection(ConfigMap edgeConfig) {
    std::string fromGroup, fromPackage, fromItem, toGroup, toPackage, toItem;
    std::vector<std::string> s;
    s = mars::utils::explodeString(':', edgeConfig["fromNode"]);
    fromGroup = s[0];
    fromPackage = "";
    for(size_t i=1; i<s.size(); ++i) {
      fromPackage.append(s[i]);
    }
    fromItem << edgeConfig["fromNodeOutput"];

    s = mars::utils::explodeString(':', edgeConfig["toNode"]);
    toGroup = s[0];
    toPackage = "";
    for(size_t i=1; i<s.size(); ++i) {
      toPackage.append(s[i]);
    }
    toItem << edgeConfig["toNodeInput"];
    fprintf(stderr, "connect: %s:%s:%s -> %s:%s:%s\n", fromGroup.c_str(),
            fromPackage.c_str(), fromItem.c_str(), toGroup.c_str(),
            toPackage.c_str(), toItem.c_str());
    dataBroker->connectDataItems(fromGroup, fromPackage, fromItem,
                                 toGroup, toPackage, toItem);
    edgeConfigList.push_back(edgeConfig);
  }

  void GraphGui::save() {
    ConfigMap map;
    {  /* save connection infos */
      std::vector<ConfigMap>::iterator it=edgeConfigList.begin();
      for(;it!=edgeConfigList.end(); ++it) {
        map["edges"] += (*it);
      }
    }

    {  /* save filter settings */
      std::vector<std::string> filterList;
      filterList = mars::utils::explodeString('\n', filterPatterns->toPlainText().toStdString());
      std::vector<std::string>::iterator it = filterList.begin();
      for(; it!=filterList.end(); ++it) {
        map["filter"] += (*it);
      }
    }

    { /* load current node positions */
      ConfigMap map;
      double x, y;
      std::string name;
      for(size_t i=0; i<nodeList.size(); ++i) {
        // store the old node positions
        name = nodeList[i]->getName();
        map["name"] = name;
        nodeList[i]->getPosition(&x, &y);
        map["pos/x"] = x;
        map["pos/y"] = y;
        nodeConfigMap[name] = map;
      }
    }

    { /* save node positions */
      std::map<std::string, ConfigMap>::iterator it = nodeConfigMap.begin();
      for(; it!=nodeConfigMap.end(); ++it) {
        map["nodes"] += it->second;
      }
    }
    map.toYamlFile(configPath+"/data_broker_graph_view.yml");    
  }

  void GraphGui::load() {
    if(mars::utils::pathExists(configPath+"/data_broker_graph_view.yml")) {
      ConfigMap map = ConfigMap::fromYamlFile(configPath+"/data_broker_graph_view.yml");
      if(map.hasKey("edges")) {
        ConfigVector::iterator it = map["edges"].begin();
        for(;it!=map["edges"].end(); ++it) {
          createDataBrokerConnection(*it);
        }
      }
      if(map.hasKey("filter")) {
        ConfigVector::iterator it = map["filter"].begin();
        std::string filter;
        for(;it!=map["filter"].end(); ++it) {
          filter += (std::string)*it + "\n";
        }
        filterPatterns->setText(QString::fromStdString(filter));
      }
      if(map.hasKey("nodes")) {
        ConfigVector::iterator it = map["nodes"].begin();
        for(;it!=map["nodes"].end(); ++it) {
          nodeConfigMap[(std::string)(*it)["name"]] = *it;
        }
      }
    }
  }

  void GraphGui::updateDataBrokerConnections() {
    std::vector<ConfigMap>::iterator it=edgeConfigList.begin();
    for(; it!=edgeConfigList.end(); ++it) {
      createConnection(*it);
    }
  }

  void GraphGui::createConnection(ConfigMap &edgeConfig) {
    std::string fromGroup, fromPackage, fromItem, toGroup, toPackage, toItem;
    std::vector<std::string> s;
    s = mars::utils::explodeString(':', edgeConfig["fromNode"]);
    fromGroup = s[0];
    fromPackage = "";
    for(size_t i=1; i<s.size(); ++i) {
      if(fromPackage.size() > 0) {
        fromPackage.append(":");
      }
      fromPackage.append(s[i]);
    }
    fromItem << edgeConfig["fromNodeOutput"];

    s = mars::utils::explodeString(':', edgeConfig["toNode"]);
    toGroup = s[0];
    toPackage = "";
    for(size_t i=1; i<s.size(); ++i) {
      if(toPackage.size() > 0) {
        toPackage.append(":");
      }
      toPackage.append(s[i]);
    }
    toItem << edgeConfig["toNodeInput"];
    fprintf(stderr, "connect: %s:%s:%s -> %s:%s:%s\n", fromGroup.c_str(),
            fromPackage.c_str(), fromItem.c_str(), toGroup.c_str(),
            toPackage.c_str(), toItem.c_str());
    dataBroker->disconnectDataItems(fromGroup, fromPackage, fromItem,
                                    toGroup, toPackage, toItem);
    dataBroker->connectDataItems(fromGroup, fromPackage, fromItem,
                                 toGroup, toPackage, toItem);
  }    

  void GraphGui::addNode(std::string name, unsigned long dataId, int flags) {
    osg_graph_viz::Node *node;
    osg_graph_viz::NodeInfo info;
    info.numInputs = 0;
    info.numOutputs = 0;
    info.map["name"] = name;
    bool found = false;
    std::vector<std::string> filterList;
    filterList = mars::utils::explodeString('\n', filterPatterns->toPlainText().toStdString());
    std::vector<std::string>::iterator st = filterList.begin();
    for(; st!=filterList.end(); ++st) {
      if(mars::utils::matchPattern(*st, info.map["name"])) {
        found = true;
        break;
      }
    }
    if(!found) return;
    const DataPackage &package = dataBroker->getDataPackage(dataId);
    for(size_t i=0; i<package.size(); ++i) {
      if(package[i].type == DOUBLE_TYPE) {
        if(flags & DATA_PACKAGE_WRITE_FLAG) {
          info.map["inputs"][info.numInputs++]["name"] = package[i].getName();
        }
        info.map["outputs"][info.numOutputs++]["name"] = package[i].getName();
      }
    }
    if(info.numInputs + info.numOutputs > 0) {
      fprintf(stderr, "create node: %s\n", info.map["name"].c_str());
      node = view->createNode(info);
      std::map<std::string, ConfigMap>::iterator jt=nodeConfigMap.find((std::string)info.map["name"]);
      if(jt != nodeConfigMap.end()) {
        node->setPosition(jt->second["pos/x"], jt->second["pos/y"]);
      }
      nodeList.push_back(node);
    }
    { // check if we have to create an edge for the node
      std::vector<ConfigMap>::iterator it=edgeConfigList.begin();
      for(; it!=edgeConfigList.end(); ++it) {
        osg_graph_viz::Node *fromNode = 0, *toNode=0;
        // find fromNode
        std::vector<osg::ref_ptr<osg_graph_viz::Node> >::iterator jt = nodeList.begin();
        for(; jt!=nodeList.end(); ++jt) {
          if((*jt)->getName() == (std::string)(*it)["fromNode"]) {
            fromNode = *jt;
          }
          else if((*jt)->getName() == (std::string)(*it)["toNode"]) {
            toNode = *jt;
          }
        }
        if(!fromNode || !toNode) {
          continue;
        }
        if(fromNode != node && toNode != node) continue;
        // find ports
        int outIdx = -1;
        for(; outIdx+1<fromNode->getNodeInfo().numOutputs; ++outIdx) {
          if(fromNode->getOutPortName(outIdx+1) == (std::string)(*it)["fromNodeOutput"]) {outIdx++; break;}
        }
        int inIdx = -1;
        for(; inIdx+1<toNode->getNodeInfo().numInputs; ++inIdx) {
          if(toNode->getInPortName(inIdx+1) == (std::string)(*it)["toNodeInput"]) {inIdx++; break;}
        }
        fprintf(stderr, "update: %d %d\n", outIdx, inIdx);
        if(outIdx < 0 || outIdx >= fromNode->getNodeInfo().numOutputs) continue;
        if(inIdx < 0 || inIdx >= toNode->getNodeInfo().numInputs) continue;
        osg_graph_viz::Edge *edge = view->createEdge(*it, outIdx, inIdx);
        fromNode->addOutputEdge(outIdx, edge);
        toNode->addInputEdge(inIdx, edge);
        fromNode->updateEdges();
        toNode->updateEdges();
        edgeList.push_back(edge);
      }      
    }
  }

  void GraphGui::receiveData(const DataInfo &info,
                             const mars::data_broker::DataPackage &dataPackage,
                             int callbackParam) {
    DataInfo newInfo;
    dataPackage.get("groupName", &newInfo.groupName);
    dataPackage.get("dataName", &newInfo.dataName);
    dataPackage.get("dataId", (long*)&newInfo.dataId);
    dataPackage.get("flags", (int*)&newInfo.flags);
    std::string nodeName = newInfo.groupName + ":" + newInfo.dataName;
    connectionMutex.lock();
    newNodes.push_back(newInfo);
    connectionMutex.unlock();
    std::vector<ConfigMap>::iterator it=edgeConfigList.begin();
    for(; it!=edgeConfigList.end(); ++it) {
      if((std::string)(*it)["fromNode"] == nodeName) {
        connectionMutex.lock();
        newConnections.push_back(*it);
        connectionMutex.unlock();
      }
      else if((std::string)(*it)["toNode"] == nodeName) {
        connectionMutex.lock();
        newConnections.push_back(*it);
        connectionMutex.unlock();
      }
    }
  }
} // end of namespace: data_broker_graph_view
