/**
 * Run parallel junction tree gibbs sampling on a factorized model
 */

#include <cstdlib>
#include <iostream>


#include <graphlab.hpp>


#include "data_structures.hpp"
#include "sequential_jt_gibbs.hpp"






#include <graphlab/macros_def.hpp>




int main(int argc, char** argv) {
  std::cout << "This program runs junction tree blocked MCMC "
            << "inference on large factorized models."
            << std::endl;

  std::string model_filename = "";

  // Command line parsing
  graphlab::command_line_options clopts("Parallel Junction Tree MCMC");
  clopts.attach_option("model", 
                       &model_filename, model_filename,
                       "Alchemy formatted model file");
  clopts.scheduler_type = "fifo";
  clopts.scope_type = "edge";
  if( !clopts.parse(argc, argv) ) { 
    std::cout << "Error parsing command line arguments!"
              << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Load alchemy file." << std::endl;
  factorized_model factor_graph;
  factor_graph.load_alchemy(model_filename);

  std::cout << "Building graphlab MRF." << std::endl;
  mrf::graph_type mrf_graph;
  construct_mrf(factor_graph, mrf_graph);

  vertex_set vset;
  for(vertex_id_t i = 0; i < mrf_graph.num_vertices(); ++i) 
    vset.insert(i);


  std::cout << "Sample one block" << std::endl;
  junction_tree::graph_type jt;


  build_junction_tree(mrf_graph, 20100, jt);

  //  for(size_t i = 0; i < 1000; ++i) {
  //    sample_once(factor_graph, mrf_graph, i);
  // }

  //sample_once(factor_graph, mrf_graph, 20100);
  
  




  return EXIT_SUCCESS;
} // end of main













#include <graphlab/macros_undef.hpp>


