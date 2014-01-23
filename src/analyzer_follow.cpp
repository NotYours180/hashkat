#include <iostream>

#include "analyzer.h"

using namespace std;

struct AnalyzerFollow {
    //** Note: Only use reference types here!!
    Network& network;
    ParsedConfig& config;
    //** Mind the StatE/StatS difference. They are entirely different structs.
    AnalysisState& state;
    AnalysisStats& stats;
    MemPoolVectorGrower& follow_set_grower;
    CategoryGrouper& follow_ranks;

    vector<double>& follow_probabilities, updating_follow_probabilities;

    EntityTypeVector& entity_types;
    MTwist& rng;
    // There are multiple 'Analyzer's, they each operate on parts of AnalysisState.
    AnalyzerFollow(AnalysisState& state) :
            network(state.network), state(state), stats(state.stats), follow_set_grower(state.follow_set_grower),
            config(state.config), follow_ranks(state.follow_ranks),
            follow_probabilities(state.follow_probabilities),
            updating_follow_probabilities(state.updating_follow_probabilities),
            entity_types(state.entity_types), rng(state.rng) {
    }

   /***************************************************************************
    * Entity mutation routines
    ***************************************************************************/
   // Return whether the follow could be added, or if we have run out of buffer room.
   bool handle_follow(int actor, int target) {
       Entity& A = network[actor];
       Entity& T = network[target];
       bool was_added = follow_set_grower.add_if_possible(A.follow_set, target);
       if (was_added) {
           was_added = follow_set_grower.add_if_possible(T.follower_set, actor);
           return true;
       }
       return false;
   }

   /***************************************************************************
    * Entity observation routines
    ***************************************************************************/

    void follow_entity(int entity, int n_entities, double time_of_follow) {
        Entity& e1 = network[entity];
        int entity_to_follow = -1;
        double rand_num = rng.rand_real_not0();
        // if we want to do random follows
        if (config.follow_model == RANDOM_FOLLOW) {
            // find a random entity within [0:number of entities - 1]
            entity_to_follow = rng.rand_int(n_entities);
        } else if (config.follow_model == PREFERENTIAL_FOLLOW) {
            // if we want to use a preferential follow method
            double sum_of_weights = 0;
            updating_follow_probabilities.resize(follow_probabilities.size());
            /* search through the probabilities for each threshold and find
               the right bin to land in */
            for (int i = 0; i < follow_probabilities.size(); i ++){
                // look at each category
                CategoryEntityList& C = follow_ranks.categories[i];
                updating_follow_probabilities[i] = follow_probabilities[i]*C.entities.size();
                sum_of_weights += C.entities.size()*follow_probabilities[i];
            }
            for (int i = 0; i < follow_probabilities.size(); i ++ ){
                updating_follow_probabilities[i] /= sum_of_weights;
            }
            for (int i = 0; i < follow_probabilities.size(); i ++) {
                if (rand_num - updating_follow_probabilities[i] <= ZEROTOL) {
                    // point to the category we landed in
                    CategoryEntityList& C = follow_ranks.categories[i];
                    // make sure we're not pulling a entity from an empty list
                    if (C.entities.size() != 0) {
                        // pull a random entity from whatever bin we landed in and break so we do not continue this loop
                        entity_to_follow = C.entities[rng.rand_int(C.entities.size())];
                        break;
                    }
                }
                // part of the above search
                rand_num -= updating_follow_probabilities[i];
            }
        } else if (config.follow_model == ENTITY_FOLLOW) {
            // if we want to follow by entity class
            /* search through the probabilities for each entity and find the right bin to land in */
            for (int i = 0; i < entity_types.size(); i++) {
                if (rand_num <= entity_types[i].prob_follow) {
                    // make sure we're not pulling from an empty list
                    if (entity_types[i].entity_list.size() != 0) {
                        // pull the entity from whatever bin we landed in and break so we dont continue this loop
                        entity_to_follow = entity_types[i].entity_list[rng.rand_int(entity_types[i].entity_list.size())];
                        break;
                    }
                }
                // part of the above search
                rand_num -= entity_types[i].prob_follow;
            }
        } else if (config.follow_model == RETWEET_FOLLOW) {
            //Retweet follow method
            if (rand_num > 0.5) {
                Retweet retweet;
                if (e1.retweets.check_recent(retweet)) {
                    // grab the latest retweet
                    // if the retweet happened in the last 48 hours
                    if (time_of_follow - retweet.time < 2880) {
                        entity_to_follow = retweet.original_tweeter;
                    }
                }
            }
            else {
                entity_to_follow = rng.rand_int(n_entities);
            }
        }

        // check and make sure we are not following ourself, or we are following entity -1
        if (LIKELY(entity != entity_to_follow && entity_to_follow != -1)) {
            // point to the entity who is being followed
            if (handle_follow(entity, entity_to_follow)) {
                // based on the number of followers the followed-entity has, check to make sure we're still categorized properly
                Entity& target = network[entity_to_follow];
                follow_ranks.categorize(entity_to_follow, target.follower_set.size);
                stats.n_follows++; // We were able to add the follow; almost always the case.
                entity_types[e1.entity].n_follows ++;
                entity_types[target.entity].n_followers ++;
            }
        }
        //
    }

    void followback(int follower, int followed) {
        // now the followee will follow the follower back
        if (handle_follow(followed, follower)) {
            Entity& target = network[follower];
            follow_ranks.categorize(follower, target.follower_set.size);
            stats.n_follows++; // We were able to add the follow; almost always the case.
        }
    }
};

void analyzer_follow_entity(AnalysisState& state, int entity, int n_entities, double time_of_follow) {
    AnalyzerFollow analyzer(state);
    analyzer.follow_entity(entity, n_entities, time_of_follow);
}

void analyzer_followback(AnalysisState& state, int follower, int followed) {
    AnalyzerFollow analyzer(state);
    analyzer.followback(follower, followed);
}