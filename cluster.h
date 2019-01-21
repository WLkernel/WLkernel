/* 
 * Copyright 2016 Emaad Ahmed Manzoor
 * License: Apache License, Version 2.0
 * http://www3.cs.stonybrook.edu/~emanzoor/streamspot/
 */

#ifndef STREAMSPOT_CLUSTER_H_
#define STREAMSPOT_CLUSTER_H_

#include <bitset>
#include "cluster.h"
#include "graph.h"
#include "param.h"
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define ANOMALY -1  // cluster ID for anomaly
#define UNSEEN -2   // cluster ID for unseen graphs

namespace std {

void hash_bands(uint32_t gid, const bitset<L>& sketch,
                vector<unordered_map<bitset<R>,vector<uint32_t>>>& hash_tables);
bool is_isolated(const bitset<L>& sketch,
                 const vector<unordered_map<bitset<R>,vector<uint32_t>>>&
                    hash_tables);
void get_shared_bucket_graphs(const bitset<L>& sketch,
                              const vector<unordered_map<bitset<R>,
                                                   vector<uint32_t>>>& hash_tables,
                              unordered_set<uint32_t>& shared_bucket_graphs);
tuple<vector<bitset<L>>, vector<vector<double>>>
construct_centroid_sketches(unordered_map<uint32_t,vector<int>>& streamhash_projections,
                            const vector<vector<uint32_t>>& bootstrap_clusters,
                            uint32_t nclusters);

void update_distances_and_clusters(uint32_t gid,
                                   const vector<int>& projection_delta,
                                   unordered_map<uint32_t,bitset<L>>& graph_sketches,
                                   unordered_map<uint32_t,vector<int>>& graph_projections,
                                   vector<bitset<L>>& centroid_sketches,
                                   vector<vector<double>>&centroid_projections,
                                   vector<uint32_t>& cluster_sizes,
                                   unordered_map<uint32_t,int32_t>& cluster_map,
                                   unordered_map<uint32_t,double>& anomaly_scores,
                                   double anomaly_threshold,
                                   const vector<double>& cluster_thresholds);
void update_distances_and_clusters_new(uint32_t gid,
                                   unordered_map<uint32_t,vector<int>>& graph_projections,
                                   unordered_map<uint32_t,bitset<L>>& graph_sketches,
                                   vector<vector<double>>&centroid_projections,
                                   vector<bitset<L>>& centroid_sketches,
                                   vector<uint32_t>& cluster_sizes,
                                   unordered_map<uint32_t,int32_t>& cluster_map,
                                   unordered_map<uint32_t,double>& anomaly_scores,
                                   unordered_map<uint32_t,vector<double>>& all_scores,
                                   const vector<double>& cluster_thresholds);

void update_frequency_distances_and_clusters(uint32_t gid,
                                   unordered_map<uint32_t,vector<uint32_t>>& graph_edgeVectors,
                                   vector<vector<double>>&centroid_edgeVectors,
                                   vector<uint32_t>& cluster_sizes,
                                   unordered_map<uint32_t,int32_t>& cluster_map,
                                   unordered_map<uint32_t,double>& anomaly_scores,
                                   const vector<double>& cluster_thresholds);


}

#endif
