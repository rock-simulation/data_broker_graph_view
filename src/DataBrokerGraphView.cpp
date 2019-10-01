/**
 * \file DataBrokerGraphView.cpp
 * \author Malte Langosz (malte.langosz@dfki.de)
 *
 **/

#include "DataBrokerGraphView.hpp"
#include "GraphGui.hpp"
#include <cstdio>
#include <iostream>

namespace data_broker_graph_view {

  using namespace mars::cfg_manager;

  DataBrokerGraphView::DataBrokerGraphView(lib_manager::LibManager* theManager) :
    lib_manager::LibInterface(theManager), gui(NULL),
    cfg(NULL), dataBroker(NULL) {

    // setup GUI with default path
    setupGUI("../resources/");
  }

  void DataBrokerGraphView::setupGUI(std::string rPath) {

    if(libManager == NULL) return;

    cfg = libManager->getLibraryAs<CFGManagerInterface>("cfg_manager");

    if(cfg) {
      cfgPropertyStruct path;
      path = cfg->getOrCreateProperty("Preferences", "resources_path", rPath);
      rPath = path.sValue;

      path = cfg->getOrCreateProperty("Config", "config_path", std::string("."));
      configPath = path.sValue;
    } else {
      fprintf(stderr, "******* gui_cfg: couldn't find cfg_manager\n");
    }

    gui = libManager->getLibraryAs<mars::main_gui::GuiInterface>("main_gui");

    if (gui == NULL)
      return;

    dataBroker = libManager->getLibraryAs<mars::data_broker::DataBrokerInterface>("data_broker");
    plugin_win = new GraphGui (cfg, dataBroker);

    //path.append("/data_broker_widget/resources/images/data_broker_symbol.png");
    /*
    std::string tmpString = configPath;
    tmpString.append("/");
    tmpString.append("DataBrokerPlotter.yaml");
    cfg->loadConfig(tmpString.c_str());
    */
    gui->addGenericMenuAction("../DataBrokerGraphView/show window", 1, this);
    //gui->addGenericMenuAction("../DataBrokerGraphView/foo", 3, this);

  }


  DataBrokerGraphView::~DataBrokerGraphView() {
    if(libManager == NULL) return;
    /*
    std::string tmpString = configPath + std::string("/DataBrokerPlotter.yaml");
    cfg->writeConfig(tmpString.c_str(), "DataBrokerPlotter");
    */
    if(cfg) libManager->releaseLibrary("cfg_manager");
    if(gui) libManager->releaseLibrary("main_gui");
    if(dataBroker) libManager->releaseLibrary("data_broker");
  }


  void DataBrokerGraphView::menuAction(int action, bool checked) {
    (void)checked;

    if (gui == NULL)
      return;

    if(action == 1) {
      plugin_win->show ();
    }
    else if(action == 2) {
      plugin_win->updatePackages();
    }
    else if(action == 3) {
      plugin_win->updateDataBrokerConnections();
    }
  }

} // end of namespace: data_broker_graph_view

DESTROY_LIB(data_broker_graph_view::DataBrokerGraphView);
CREATE_LIB(data_broker_graph_view::DataBrokerGraphView);
