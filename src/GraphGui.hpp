/**
 * \file GraphGui.hpp
 * \author Malte Langosz (malte.langosz@dfki.de)
 * \brief
 *
 * Version 0.1
 */

#ifndef DATA_BROKER_GRAPH_GUI_H
#define DATA_BROKER_GRAPH_GUI_H

#include <mars/main_gui/BaseWidget.h>
#include <QTextEdit>
#include <QPushButton>
#include <QMutex>
#include <osg_graph_viz/View.hpp>
#include <osgViewer/CompositeViewer>
#include <mars/data_broker/DataBrokerInterface.h>
#include <mars/data_broker/ReceiverInterface.h>

namespace data_broker_graph_view {
  class GraphGui : public mars::main_gui::BaseWidget,
                   public mars::data_broker::ReceiverInterface,
                   public osg_graph_viz::UpdateInterface {
Q_OBJECT
  public:
    GraphGui (mars::cfg_manager::CFGManagerInterface *cfg,
              mars::data_broker::DataBrokerInterface *dataBroker);
    virtual ~GraphGui ();
    void newEdge(osg_graph_viz::Edge *edge, osg_graph_viz::Node* fromNode,
                 int fromNodeIdx,
                 osg_graph_viz::Node* toNode, int toNodeIdx);
    bool removeEdge(osg_graph_viz::Edge* edge);
    void updateDataBrokerConnections();
    void receiveData(const mars::data_broker::DataInfo &info,
                     const mars::data_broker::DataPackage &data_package,
                     int callbackParam);
    osg_graph_viz::Node* addNode(configmaps::ConfigMap node) {return 0;}
    void addEdge(configmaps::ConfigMap edge, bool r) {}
  public slots:
    void save();
    void updatePackages();

  protected:
    void timerEvent(QTimerEvent *event);

  private:
    QMutex connectionMutex;
    QPushButton *btn;
    QTextEdit *filterPatterns;
    osg::ref_ptr<osg_graph_viz::View> view;
    osgViewer::CompositeViewer *viewer;
    mars::data_broker::DataBrokerInterface *dataBroker;
    std::vector<osg::ref_ptr<osg_graph_viz::Node> > nodeList;
    std::vector<osg::ref_ptr<osg_graph_viz::Edge> > edgeList;
    std::vector<configmaps::ConfigMap> edgeConfigList;
    std::map<std::string, configmaps::ConfigMap> nodeConfigMap;
    std::vector<configmaps::ConfigMap> newConnections;
    std::vector<mars::data_broker::DataInfo> newNodes;
    std::string configPath;
    bool ignoreRemoveEdge;
    void createDataBrokerConnection(configmaps::ConfigMap edgeConfig);
    void createConnection(configmaps::ConfigMap &edgeConfig);
    void addNode(std::string name, unsigned long dataId, int flags);
    void load();
  };
} // end of namespace: data_broker_graph_view

#endif // DATA_BROKER_GRAPH_GUI_H
