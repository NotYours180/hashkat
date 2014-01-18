#include <string>
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <deque>

// Local includes:
#include "analyzer.h"
#include "util.h"
#include "MemPoolVector.h"
#include "network.h"

#include "dependencies/lcommon/Timer.h"

#include <signal.h>

using namespace std;

volatile int CTRL_C_ATTEMPTS = 0;

static const int CTRL_C_ATTEMPTS_TO_ABORT = 3;
// Handler for SIGINT -- sent by Ctrl-C on command-line. Allows us to stop our program gracefully!
static void ctrl_C_handler(int __dummy) {
    CTRL_C_ATTEMPTS++;
    if (CTRL_C_ATTEMPTS >= CTRL_C_ATTEMPTS_TO_ABORT) {
        error_exit("User demands abort!");
        signal(SIGINT, SIG_DFL);
    }
}
static void ctrl_C_handler_install() {
   signal(SIGINT, ctrl_C_handler);
}

/* The Analyze struct encapsulates the many-parameter analyze function, and its state. */
struct Analyzer {

    /***************************************************************************
     * Configuration and state variables of the analysis routines.
     ***************************************************************************/

    AnalysisState& state;
    ParsedConfig& config;
    AnalysisStats& stats;

    // struct for the entity classes, see network.h for specifications
    EntityTypeVector& entity_types;

    // The network state
    Network& network;
    MemPoolVectorGrower& follow_set_grower;

	// Categories for tweeting, following, retweeting
    CategoryGrouper& tweet_ranks;
	CategoryGrouper& follow_ranks;
	CategoryGrouper& retweet_ranks;

    /* Mersenne-twister random number generator */
    MTwist& rng;

    // Times the interval between prints to the console
    Timer stdout_milestone_timer;

    ofstream DATA_TIME; // Output file to plot data

    double& time;

    /***************************************************************************
     * Initialization functions
     ***************************************************************************/

    /* Initialization and loading of configuration.
     * Reads configuration from the given input file. */
    Analyzer(AnalysisState& state) :
        //** Here, we initialize REFERENCES to the various analysis pieces.
        //** The only way to initialize references is through this construct -- called an 'initializer list'.
        //** This ensures that updates are witnessed in the caller's AnalysisState object.
            state(state), stats(state.stats), config(state.config),
            entity_types(state.entity_types), network(state.network),
            follow_set_grower(state.follow_set_grower),
            tweet_ranks(state.tweet_ranks), follow_ranks(state.follow_ranks), retweet_ranks(state.retweet_ranks),
            rng(state.rng), time(state.time) {

        // The following allocates a memory chunk proportional to max_entities:
        network.preallocate(config.max_entities);
        follow_set_grower.preallocate(FOLLOW_SET_MEM_PER_USER * config.max_entities);

        DATA_TIME.open("DATA_vs_TIME");

		set_initial_entities();
        call_future_rates();
		analyzer_rate_update(state);
    }
    // make sure any initial entities are given a title based on the respective probabilities
	void call_future_rates() {
		for (int i = 0; i < entity_types.size(); i ++) {
			for (int j = 0; j < entity_types[i].number_of_events; j ++) {
				set_future_rates(entity_types[i], j);
			}
		}
	}
	void set_future_rates(EntityType& et, int event) {
		int projected_months = config.max_time/TIME_CAT_FREQ;
		if (et.RF[event].function_type == "constant") {
			ASSERT(et.RF[event].const_val >= 0, "Check your rates, one of them is < 0");
			for (int i = 0; i < projected_months; i ++) {
				et.RF[event].monthly_rates.push_back(et.RF[event].const_val);
			}
		} else if (et.RF[event].function_type == "linear") {
			for (int i = 0; i < projected_months; i ++) {
				if (et.RF[event].y_intercept + i * et.RF[event].slope >= 0) {
					et.RF[event].monthly_rates.push_back(et.RF[event].y_intercept + i * et.RF[event].slope);
				} else {
					et.RF[event].monthly_rates.push_back(0);
				}
			}
		} else if (et.RF[event].function_type == "exponential") {
			for (int i = 0; i < projected_months; i ++) {
				if (et.RF[event].amplitude*exp(et.RF[event].exp_factor) >= 0) {
					et.RF[event].monthly_rates.push_back(et.RF[event].amplitude*exp(et.RF[event].exp_factor));
				} else {
					et.RF[event].monthly_rates.push_back(0);
				}
			}
		}
		
	}
    void set_initial_entities() {
        for (int i = 0; i < config.initial_entities; i++) {
             action_create_entity(0.0, i);
        }
    }

    /***************************************************************************
     * Analysis routines
     ***************************************************************************/

    // Entry point to all analysis:
    /* Conceptually this is the true entry point to our program,
     * after the messy configuration and allocation is done.
     * Returns end-time. */
    double main() {
        run_network_simulation();
        return time;
    }

    // ROOT ANALYSIS ROUTINE
    /* Run the main analysis routine using this config. */
    void run_network_simulation() {
        while ( LIKELY(time < config.max_time && network.n_entities < config.max_entities && CTRL_C_ATTEMPTS == 0) ) {
        	step_analysis();
        }
    }

    void step_time(int n_entities) {
        static double STATIC_TIME = 0.0;

        double prev_time = time;
        double prev_integer = floor(time);
        double increment = -log(rng.rand_real_not0()) / stats.event_rate;
        if (config.use_random_increment) {
            // increment by random time
            time += increment;
        } else {
            time += 1.0 / stats.event_rate;
        }

		    // ASSERT(STATIC_TIME < time, "Fail");
        STATIC_TIME = time;
        if (config.output_stdout_summary && (floor(time) > prev_integer)) {
          output_summary_stats();
        }
    }

    /* Create a entity at the given index.
     * The index should be an empty entity slot. */
    void action_create_entity(double creation_time, int index) {
		Entity& e = network[index];
		e.creation_time = creation_time;
		double rand_num = rng.rand_real_not0();
		for (int et = 0; et < entity_types.size(); et++) {
			if (rand_num <= entity_types[et].prob_add) {
				e.entity = et;
                entity_types[et].entity_list.push_back(index);
				follow_ranks.categorize(index, e.follower_set.size);
				break;
			}
			rand_num -= entity_types[et].prob_add;
		}
		if (config.use_barabasi){
			analyzer_follow_entity(state, index, index, creation_time);
		}
        network.n_entities++;
    }

	// function to handle the tweeting
	void action_tweet(int entity) {
		// This is the entity tweeting
		Entity& e = network[entity];
		// increase the number of tweets the entity had by one
		e.n_tweets++;
		tweet_ranks.categorize(entity, e.n_tweets);
//		if (e.n_tweets > 1000) {
//			action_unfollow(entity);
//		}
	}
	
	void action_retweet(int entity, double time_of_retweet) {
		Entity& retweetee = network[entity];
		FollowList& f = retweetee.follow_set;
		int entity_retweeted = -1;
		double rand_num = rng.rand_real_not0();
		if (rand_num < 0.5) {
			if (f.size != 0){
				entity_retweeted = f[rng.rand_int(f.size)];
			}
		}
		else {
			// retweet a retweet
		    Retweet retweet;
		    if (retweetee.retweets.check_recent(retweet)) {
		        // grab the latest retweet
				// if the retweet happened in the last 48 hours
				if (time_of_retweet - retweet.time < 2880) {
					entity_retweeted = retweet.original_tweeter;
				}
			}
		}
		if (entity_retweeted != -1 /*No retweet should occur*/) {
			int n_following = network.n_following(entity);
            // Loop over all the entities that witness the event:
			for (int i = 0; i < n_following; i ++){
				Entity& audience = network[network.follow_i(entity,i)];
				Retweet retweet(entity_retweeted, time_of_retweet);
				audience.retweets.add(retweet);
			}
			retweetee.n_retweets ++;
			stats.n_retweets ++;
		}
	}

	void action_unfollow(int entity_id) {
	    // Broken, commented out -- AD
//		Entity& e1 = network[entity_id];
//		int unfollowing_location = rng.rand_int(network.n_followers(entity_id));
//		Entity& e2 = network[unfollowing_location];
//		FollowerList& f1 = e1.follower_set;
//		FollowList& f2 = e2.follow_set;
		// remove unfollowing location from follower_list
		// remove entity id from follow_list
		
	}

    // Performs one step of the analysis routine.
    // Takes old time, returns new time
    void step_analysis() {
        double u_1 = rng.rand_real_not0(); // get the first number with-in [0,1).
        int N = network.n_entities;

        // DECIDE WHAT TO DO:
        if (u_1 - (stats.prob_add) <= ZEROTOL) {
                        // If we find ourselves in the add entity chuck of our cumulative function:
            action_create_entity(time, N);
        } else if (u_1 - (stats.prob_add + stats.prob_follow) <= ZEROTOL) {
            int entity = analyzer_select_entity(state, FOLLOW_SELECT);
            analyzer_follow_entity(state, entity, N, time);
        } else if (u_1 - (stats.prob_add + stats.prob_follow + stats.prob_tweet) <= ZEROTOL) {
                        // The tweet event
            int entity = analyzer_select_entity(state, TWEET_SELECT);
            action_tweet(entity);
            stats.n_tweets ++;
        } else if (u_1 - (stats.prob_add + stats.prob_follow + stats.prob_tweet + stats.prob_norm) <= ZEROTOL ) {
            int entity = analyzer_select_entity(state, RETWEET_SELECT);
            action_retweet(entity, time);
        } else {
            error_exit("step_analysis: event out of bounds");
        }

        step_time(N);
        stats.n_steps++;
        //update the rates if n_entities has changed
        analyzer_rate_update(state);
    }

    /***************************************************************************
     * Helper functions
     ***************************************************************************/

    void output_summary_stats(ostream& stream, double time_spent = /*Don't print*/-1) {
        stream << fixed << setprecision(2)
                << time << "\t\t"
                << network.n_entities << "\t\t"
                << stats.n_follows << "\t\t"
                << stats.n_tweets << "\t\t"
                << stats.n_retweets << "\t\t"
                << stats.event_rate << "\t";
        if (time_spent != -1) {
            stream << time_spent << "ms\t";
        }
        stream << "\n";
    }
    void output_summary_stats() {

		const char* HEADER = "\n#Time\t\tUsers\t\tFollows\t\tTweets\t\tRetweets\tR\tReal Time Spent\n\n";
        if (stats.n_outputs % (25 * STDOUT_OUTPUT_RATE) == 0) {
        	cout << HEADER;
        }
        if (stats.n_outputs % 500 == 0) {
            DATA_TIME << HEADER;
        }

        output_summary_stats(DATA_TIME);
        if (stats.n_outputs % STDOUT_OUTPUT_RATE == 0) {
        	output_summary_stats(cout, stdout_milestone_timer.get_microseconds() / 1000.0);
        	stdout_milestone_timer.start(); // Restart the timer
        }

        stats.n_outputs++;
    }
};

// Run a network simulation using the given input file's parameters
void analyzer_main(AnalysisState& state) {
    Timer timer;
    Analyzer analyzer(state);
    if (state.config.handle_ctrlc) {
        ctrl_C_handler_install();
    }
    analyzer.main();
    signal(SIGINT, SIG_DFL);
    printf("'analyzer_main' took %.2f milliseconds.", timer.get_microseconds() / 1000.0);
}
