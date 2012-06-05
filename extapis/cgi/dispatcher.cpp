/**  
 * Copyright (c) 2009 Carnegie Mellon University. 
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://www.graphlab.ml.cmu.edu
 *
 */
 
/**
 * @file dispatcher.cpp
 * @author Jiunn Haur Lim <jiunnhal@cmu.edu>
 */
 
#include "dispatcher.hpp"
#include "process.hpp"
#include "json_message.hpp"

#include <graphlab/macros_def.hpp>

using namespace graphlab;
typedef json_invocation ji;
typedef json_return     jr;

///////////////////////////////// CGI_GATHER ///////////////////////////////////
cgi_gather::cgi_gather(const std::string& state) : mstate(state){}

void cgi_gather::save(oarchive& oarc) const {
  oarc << mstate;
}

void cgi_gather::load(iarchive& iarc) {
  iarc >> mstate;
}

void cgi_gather::operator+=(const cgi_gather& other){
  
  // invoke
  process& p = process::get_process();
  ji invocation("merge", mstate);
  invocation.add_other(other.mstate);
  p.send(invocation);
  
  // receive
  jr result;
  p.receive(result);
  
  // update states (if none received, no change)
  const char *cstring = result.program();
  if (NULL != cstring)
    mstate = std::string(cstring);
  
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// DISPATCHER //////////////////////////////////
dispatcher::dispatcher(const std::string& state) : mstate(state){}

void dispatcher::save(oarchive& oarc) const {
  oarc << mstate;
}

void dispatcher::load(iarchive& iarc){
  iarc >> mstate;
}

/** initialize the vertex program and vertex data */
void dispatcher::init(icontext_type& context, vertex_type& vertex) { 

  // invoke
  process& p = process::get_process();
  ji invocation("init", mstate);
  p.send(invocation);
  
  // receive
  jr result;
  p.receive(result);

  // update vertex program state (if none received, defaults to "")
  const char *cstring = result.program();
  mstate = std::string(cstring);
  
  // update vertex state (if none received, no change)
  cstring = result.vertex();
  if (NULL != cstring)
    vertex.data() = std::string(cstring);
    
}

edge_dir_type dispatcher::gather_edges
                             (icontext_type& context,
                              const vertex_type& vertex) const {
  
  // invoke
  process& p = process::get_process();
  ji invocation("gather_edges", mstate);
  invocation.add_vertex(vertex);
  p.send(invocation);
  
  // receive
  jr result;
  p.receive(result);
  
  return result.edge_dir();
  
}

edge_dir_type dispatcher::scatter_edges
                            (icontext_type& context,
                             const vertex_type& vertex) const {
  
  // invoke
  process& p = process::get_process();
  ji invocation("scatter_edges", mstate);
  invocation.add_vertex(vertex);
  p.send(invocation);
  
  // receive
  jr result;
  p.receive(result);
  
  return result.edge_dir();
  
}

dispatcher::gather_type dispatcher::gather
                             (icontext_type& context,
                              const vertex_type& vertex,
                              edge_type& edge) const {

  // invoke
  process& p = process::get_process();
  ji invocation("gather", mstate);
  invocation.add_vertex(vertex);
  invocation.add_edge(edge);
  p.send(invocation);
  
  // receive
  jr result;
  p.receive(result);
  
  // return result
  const char *cstring = result.result();
  return cgi_gather(std::string(cstring));

}

void dispatcher::apply(icontext_type& context,
                       vertex_type& vertex,
                       const gather_type& total) {

  // invoke
  process &p = process::get_process();
  ji invocation("apply", mstate);
  invocation.add_vertex(vertex);
  invocation.add_gather(total);
  p.send(invocation);
  
  // receive
  jr result;
  p.receive(result);
  
  // update states (null means no change)
  const char *cstring = result.program();
  if (NULL != cstring)
    mstate = std::string(cstring);                      // copy
  cstring = result.vertex();
  if (NULL != cstring)
    vertex.data() = std::string(cstring);               // copy

}

void dispatcher::scatter(icontext_type& context,
                         const vertex_type& vertex,
                         edge_type& edge) const {
                         
  process& p = process::get_process();
  
  // invoke
  ji invocation("scatter", mstate);
  invocation.add_vertex(vertex);
  invocation.add_edge(edge);
  p.send(invocation);
  
  // parse results
  jr result;
  p.receive(result);

  // null means no signal
  const char *cstring = result.signal();
  if (NULL == cstring) return;

  if (strcmp("SOURCE", cstring)){
    context.signal(edge.source());
  }else if (strcmp("TARGET", cstring)){
    context.signal(edge.target());
  }else throw("unrecognized signal");
  
}
////////////////////////////////////////////////////////////////////////////////

struct dispatcher_writer {
  std::string save_vertex(dispatcher::vertex_type vertex) {
    std::stringstream strm;
    strm << ":" << vertex.id() << "\t" << vertex.data() << "\n";
    return strm.str();
  }
  std::string save_edge(dispatcher::edge_type edge) {
    std::stringstream strm;
    strm << edge.source().id() << "\t" << edge.target().id() << "\t" << edge.data() << "\n";
    return strm.str();
  }
};

////////////////////////////////// MAIN METHOD /////////////////////////////////
int main(int argc, char** argv) {

  // Initialize control plain using mpi
  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  
  // Parse command line options ------------------------------------------------
  command_line_options clopts("GraphLab Dispatcher");
  
  std::string graph_dir = "toy.tsv";
  clopts.attach_option("graph", &graph_dir, graph_dir,
                       "The graph file.  If none is provided "
                       "then a toy graph will be created");
  clopts.add_positional("graph");
  std::string format = "tsv";
  clopts.attach_option("format", &format, format,
                       "The graph file format: {metis, snap, tsv, adj, bin}");
  std::string program;
  clopts.attach_option("program", &program,
                       "The program to execute (required)");
  std::string saveprefix;
  clopts.attach_option("saveprefix", &saveprefix, saveprefix,
                       "If set, will save the resultant graph to a "
                       "sequence of files with prefix saveprefix");
                       
  if(!clopts.parse(argc, argv)) {
    std::cout << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }
  
  if(!clopts.is_set("program")) {
    std::cout << "Updater not provided." << std::endl;
    clopts.print_description();
    return EXIT_FAILURE;
  }
  
  process::set_executable(program);
  
  // Signal Handling -----------------------------------------------------------
  struct sigaction sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  CHECK(!::sigaction(SIGPIPE, &sa, NULL));
  
  // Load graph ----------------------------------------------------------------
  std::cout << dc.procid() << ": Starting." << std::endl;
  dispatcher::graph_type graph(dc, clopts);
  graph.load_format(graph_dir, format);
  graph.finalize();
  
  std::cout << "#vertices: " << graph.num_vertices() << " #edges:" << graph.num_edges() << std::endl;
  
  // Schedule vertices
  std::cout << dc.procid() << ": Creating engine" << std::endl;
  async_consistent_engine<dispatcher> engine(dc, graph, clopts);
  // TODO: need a way for user to tell us which vertices to schedule
  std::cout << dc.procid() << ": Scheduling all" << std::endl;
  engine.signal_all();
  
  // Run the dispatcher --------------------------------------------------------
  engine.start();

  // Save output ---------------------------------------------------------------
  if(!clopts.is_set("saveprefix"))
    graph.save(saveprefix, dispatcher_writer(), false);
  
  mpi_tools::finalize();
  return EXIT_SUCCESS;
  
}
////////////////////////////////////////////////////////////////////////////////

#include <graphlab/macros_undef.hpp>
