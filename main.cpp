#include <stdio.h>
#include <algorithm>
#include <bitset>
#include <cassert>
#include <chrono>
#include <deque>
#include <iostream>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cluster.h"
#include "docopt.h"
#include "graph.h"
#include "hash.h"
#include "io.h"
#include "param.h"
#include "simhash.h"
#include "streamhash.h"

//#define DEBUG
using namespace std;

static const char USAGE[] =
R"(StreamSpot.

    Usage:
        streamspot --edges=<edge file>
                --chunk-length=<chunk length>

    streamspot (-h | --help)

    Options:
        -h, --help                              Show this screen.
        --edges=<edge file>                     Incoming stream of edges.
        --chunk-length=<chunk length>           Parameter C.
)";

void allocate_random_bits(vector<vector<uint64_t>>& H, mt19937_64& prng,uint32_t chunk_length);

int main(int argc,char *argv[])
{
    vector<vector<uint64_t>> H(L);                 // Universal family H, contains L hash functions, 
    mt19937_64 prng(SEED);                         // Mersenne Twister 64-bit PRNG
    bernoulli_distribution bernoulli(0.5);         // to generate random vectors
    vector<vector<int>> random_vectors(L);         // |S|-element random vectors

    // for timing
    chrono::time_point<chrono::steady_clock> start;
    chrono::time_point<chrono::steady_clock> end;
    chrono::nanoseconds diff;
	chrono::nanoseconds WLkernal_construction_times=chrono::nanoseconds(0);
	chrono::nanoseconds sketch_construction_times=chrono::nanoseconds(0);
	chrono::nanoseconds cluster_update_times=chrono::nanoseconds(0);
	long graphNum=0;//for average time

    // arguments
    map<string, docopt::value> args = docopt::docopt(USAGE, { argv + 1, argv + argc });

    string edge_file(args["--edges"].asString());
    uint32_t chunk_length =args["--chunk-length"].asLong();

	uint32_t nclusters =5;//5 benign clusters except 300-399 for attack
	vector<vector<uint32_t>> clusters(nclusters);
    vector<double> cluster_thresholds;


    unordered_set<uint32_t> train_gids;
	vector<uint32_t> cluster_sizes(nclusters,0);
    for (uint32_t i = 0; i < nclusters; i++) {// 5 benign cluster
		if(i<3){
    		cluster_sizes[i] =TRAIN_PERCENT;
        	for (uint32_t j = 0; j < TRAIN_PERCENT; j++) {
            	train_gids.insert(100*i+j);
				clusters[i].push_back(100*i+j);
        	}
        }else{//300-399 is attack scenario
         	cluster_sizes[i] =TRAIN_PERCENT;
        	for (uint32_t j = 0; j < TRAIN_PERCENT; j++) {
            	train_gids.insert(100*(i+1)+j);
				clusters[i].push_back(100*(i+1)+j);
        	}
        }
    }

    vector<edge> train_edges;
    unordered_map<uint32_t,graph> train_graphs;
    unordered_map<uint32_t,vector<int>> train_projections;
    unordered_map<uint32_t,bitset<L>> train_sketches;
    train_edges =read_edges(edge_file, train_gids);

    cout << "Constructing " << train_gids.size() << " training graphs:" << endl;
    for (auto& e : train_edges) {
        update_graphs(e, train_graphs);
    }

    // set up universal hash family for StreamHash
    allocate_random_bits(H, prng, chunk_length);
    cout << "Constructing StreamHash sketches for training graphs:" << endl;
    for (auto& gid : train_gids) 
    {
		//Below WL_ITERATION iters actually responding to WL_ITERATION+1 iter in WL kernal, 
		// WL_ITERATION+1 hop is the longest subtree path. 
		unordered_map<string,uint32_t> temp_WLKernal_vector =
            construct_WLKernal_vector(train_graphs[gid], chunk_length,WL_ITERATION);
        tie(train_sketches[gid], train_projections[gid]) =
            construct_streamhash_sketch(temp_WLKernal_vector, H);
		temp_WLKernal_vector.clear();
    }

    // per-cluster data structures
    vector<vector<double>> centroid_projections;
    vector<bitset<L>> centroid_sketches;

    // construct cluster centroid sketches/projections
    cout << "Constructing bootstrap cluster centroids:" << endl;
    tie(centroid_sketches, centroid_projections) =
        construct_centroid_sketches(train_projections, clusters,nclusters);

	//caculate threshold by 3 standard deviations from mean distance to centroid
	for (uint32_t i = 0; i < nclusters; i++) {//5 benign cluster
		vector<double> distances(TRAIN_PERCENT,0);
		double sum_distance=0;
		double sum_variance=0;
		double max_distance=0;
		
		if(i<3){
			for (uint32_t j = 0; j < TRAIN_PERCENT; j++) {
				distances[j]=1.0-cos(PI*(1.0 - streamhash_similarity(train_sketches[100*i+j],centroid_sketches[i])));
				sum_distance+=distances[j];
				max_distance=distances[j]>max_distance?distances[j]:max_distance;
			}

        }else{//300-399 is attack scenario
			for (uint32_t j = 0; j < TRAIN_PERCENT; j++) {
				distances[j]=1.0-cos(PI*(1.0 - streamhash_similarity(train_sketches[100*(i+1)+j],centroid_sketches[i])));
				sum_distance+=distances[j];
				max_distance=distances[j]>max_distance?distances[j]:max_distance;
			}
        }

		double mean_distance=sum_distance/TRAIN_PERCENT;
		cout<<"mean_distance"<<mean_distance<<endl;
		for (uint32_t j = 0; j < TRAIN_PERCENT; j++) {
			sum_variance += pow(distances[j] - mean_distance, 2);
		}
		double variance=sum_variance/TRAIN_PERCENT;
		double stdDeviations=sqrt(variance);
		cout<<"stdDeviation"<<stdDeviations<<endl;
		
		sort(distances.begin(), distances.end());
		cout<<"95_distance"<<distances[(int)TRAIN_PERCENT*0.95]<<endl;
		cout<<"max_distance"<<max_distance<<endl;
		cout<<"2_stdDev_distance"<<mean_distance+2*stdDeviations<<endl;
		cout<<"3_stdDev_distance"<<mean_distance+3*stdDeviations<<endl<<endl;
		cluster_thresholds.push_back(mean_distance+3*stdDeviations);
		//cluster_thresholds.push_back(max_distance);
	}

	//erase useless data structure
	train_projections.clear();
	train_sketches.clear();
	train_graphs.clear();
	vector<edge>().swap(train_edges);

    cout<<"cluster_thresholds:"<<endl;
    for(auto&holds:cluster_thresholds)
    {
        cout<<holds<<" ";
    }
    cout<<endl;

    char data[500]={};
    uint32_t i = 0;
    char src_type, dst_type, e_type;
    edge e;

    // per-graph data structures
    unordered_map<uint32_t,graph> gidToGraph;
    unordered_map<uint32_t,bitset<L>> streamhash_sketches;
    unordered_map<uint32_t,vector<int>> streamhash_projections;
    unordered_map<uint32_t,double> anomaly_scores;//for distance to nearest cluster
    unordered_map<uint32_t,vector<double>> all_scores;//for distances to all clusters
    unordered_map<uint32_t,int32_t> cluster_map;

	int normalError=0;//statistic for normal cluster map 
	int abnormalError=0;//statistic for abnormal cluster map
	int errorAbnormal=0;//statistic for error abnormal cluster map
	double cumulative_actual_distance=0;//cumulative distance to the actual cluster

    cout<<"Waitting for graph stream:<<<<<<<<<<<<<<<<<<<<<"<<endl;
    while(fgets(data,500,stdin))
    {
        i=0;
        uint32_t src_id = data[i] - '0';
        while (data[++i] != DELIMITER) {
            src_id = src_id * 10 + (data[i] - '0');
        }
        i++; // skip delimiter
        src_type = data[i];i += 2; // skip delimiter
        uint32_t dst_id = data[i] - '0';
        while (data[++i] != DELIMITER) {
            dst_id = dst_id * 10 + (data[i] - '0');
        }
        i++; // skip delimiter
        dst_type = data[i];i += 2; // skip delimiter
        e_type = data[i];i += 2; // skip delimiter
        if(data[i]=='-'){//gid=-gid representing the graph is ended.
            uint32_t gid = data[++i] - '0';
            while (data[++i] != '\n') {
                gid = gid * 10 + (data[i] - '0');
            }
 
			if(train_gids.find(gid)!=train_gids.end())            
        	{
        		continue;
        	}
 
			start = chrono::steady_clock::now();
			unordered_map<string,uint32_t> streamhash_WLKernal_vector =
            	construct_WLKernal_vector(gidToGraph[gid], chunk_length,WL_ITERATION);
			end = chrono::steady_clock::now();
			diff = chrono::duration_cast<chrono::nanoseconds>(end - start);
			WLkernal_construction_times+=diff;

			start = chrono::steady_clock::now();
			tie(streamhash_sketches[gid], streamhash_projections[gid]) =
            	construct_streamhash_sketch(streamhash_WLKernal_vector, H);
			end = chrono::steady_clock::now();
			diff = chrono::duration_cast<chrono::nanoseconds>(end - start);
			sketch_construction_times+=diff;
			
			// update centroids and centroid-graph distances
			start = chrono::steady_clock::now();
            update_distances_and_clusters_new(gid,streamhash_projections,streamhash_sketches,\
				centroid_projections,centroid_sketches,\
				cluster_sizes, cluster_map,anomaly_scores,all_scores,cluster_thresholds);
            end = chrono::steady_clock::now();
            diff = chrono::duration_cast<chrono::nanoseconds>(end - start);
			cluster_update_times+=diff;

			graphNum++;//for average time
			
            cout<<gid<<"\tanomaly_score: "<<anomaly_scores[gid]<<"\tcluster: "<<cluster_map[gid]\
				<<"\t"<<all_scores[gid][0]<<"  "<<all_scores[gid][1]<<"  "<<all_scores[gid][2]\
				<<"  "<<all_scores[gid][3]<<"  "<<all_scores[gid][4]<<endl;
			if(gid<300){
				cumulative_actual_distance+=all_scores[gid][gid/100];
				if(cluster_map[gid]==-1){
					errorAbnormal++;
				}
				if(cluster_map[gid]!=(int32_t)gid/100){
					normalError++;
					cerr<<gid<<"\tcluster: "<<cluster_map[gid]<<"\tanomaly_score: "<<anomaly_scores[gid]\
						<<"\tactual_score:"<<all_scores[gid][gid/100]<<endl;
				}
			}else if((400>gid)&&(gid>=300)){
				if(cluster_map[gid]!=-1){
					abnormalError++;
				}
			}
			else if(gid>=400){
				cumulative_actual_distance+=all_scores[gid][gid/100-1];
				if(cluster_map[gid]==-1){
					errorAbnormal++;
				}
				if(cluster_map[gid]+1!=(int32_t)gid/100){
					normalError++;
					cerr<<gid<<"\tcluster: "<<cluster_map[gid]<<"\tanomaly_score: "<<anomaly_scores[gid]\
						<<"\tactual_score:"<<all_scores[gid][gid/100-1]<<endl;
				}
			}
			streamhash_WLKernal_vector.clear();
			streamhash_projections.erase(gid);
			streamhash_sketches.erase(gid);
			gidToGraph[gid].clear();
			cluster_map.erase(gid);
			anomaly_scores.erase(gid);
			all_scores.erase(gid);
            gidToGraph.erase(gid);
        }
        else
        {
            uint32_t gid = data[i] - '0';
            while (data[++i] != '\n') {
                gid = gid * 10 + (data[i] - '0');
            }
			if(train_gids.find(gid)!=train_gids.end())			  
			{
				continue;
			}

            //update graphs
            gidToGraph[gid][make_pair(src_id,src_type)].push_back(make_tuple(dst_id,dst_type,e_type));

        }
    }
	cout<<"normal accurate:"<<100*(1-(double)normalError/(500-5*TRAIN_PERCENT))<<"%"<<endl;
	cout<<"precision rate:"<<100*((double)(100-abnormalError)/(100-abnormalError+errorAbnormal))<<"%"<<endl;
	cout<<"recall rate:"<<100*((double)(100-abnormalError)/100)<<"%"<<endl;
	cout<<"average actual distance:"<<cumulative_actual_distance/(500-5*TRAIN_PERCENT)<<endl;
	cout<<"average shingle construction time:"<<static_cast<double>((WLkernal_construction_times/graphNum).count())<<"ns"<<endl;
	cout<<"average sketch_construction_times:"<<static_cast<double>((sketch_construction_times/graphNum).count())<<"ns"<<endl;
	cout<<"average cluster update time:"<<static_cast<double>((cluster_update_times/graphNum).count())<<"ns"<<endl;
    return 0;
}
void allocate_random_bits(vector<vector<uint64_t>>& H, mt19937_64& prng,
        uint32_t chunk_length) 
{//allocate random bits for hashing
    for (uint32_t i = 0; i < L; i++) 
    {// hash function h_i \in H
        H[i] = vector<uint64_t>(chunk_length + 2);
        for (uint32_t j = 0; j < chunk_length + 2; j++) 
        {// random number m_j of h_i
            H[i][j] = prng();
        }
    }
}
