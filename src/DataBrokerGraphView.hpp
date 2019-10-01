/**
 * \file DataBrokerGraphView.hpp
 * \author Malte Langosz (malte.langosz@dfki.de)
 *
 **/

#ifndef DATA_BROKER_GRAPH_VIEW_H
#define DATA_BROKER_GRAPH_VIEW_H

#ifdef _PRINT_HEADER_
#warning "DataBrokerGraphView.h"
#endif

#include <mars/main_gui/GuiInterface.h>
#include <mars/main_gui/MenuInterface.h>
#include <lib_manager/LibInterface.hpp>
#include <mars/data_broker/DataBrokerInterface.h>
#include <mars/cfg_manager/CFGManagerInterface.h>
#include <string>

namespace data_broker_graph_view {

  class GraphGui;

  class DataBrokerGraphView : public lib_manager::LibInterface,
                              public mars::main_gui::MenuInterface {

    public:
    DataBrokerGraphView(lib_manager::LibManager* theManager);
    void setupGUI(std::string rPath = std::string("."));
   
    virtual ~DataBrokerGraphView(void);
  
    // MenuInterface methods
    virtual void menuAction(int action, bool checked = false);

    // LibInterface methods
    int getLibVersion() const {return 1;}
    const std::string getLibName() const {
      return std::string("data_broker_graph_view");
    }
    CREATE_MODULE_INFO();

  private:
    mars::main_gui::GuiInterface* gui;
    mars::cfg_manager::CFGManagerInterface *cfg;
    mars::data_broker::DataBrokerInterface *dataBroker;
    std::string configPath;
    GraphGui *plugin_win;

  };

} // end of namespace: data_broker_graph_view

#endif // DATA_BROKER_GRAPH_VIEW_H
