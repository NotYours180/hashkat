#include <iostream>    // input/output
#include <fstream>     // input/output files
#include <vector>      // for dynamic array allocations
#include <cmath>       // for log function
#include <time.h>
#include <stdlib.h>	
#include "INFILE.h"
#include <iomanip>

using namespace std;    // use the above libraries


int main()    // this is the main function, returns 0 always
{
	//GET INFO FROM INFILE
    //#####################################################

	long long int N_USERS = INFILE("N_USERS");
	int MAX_USERS = INFILE("MAX_USERS");
	int MAX_FOLLOWING = INFILE("MAX_FOLLOWING");
	int OUTPUT = INFILE("OUTPUT");	
	int RANDOM_INCR = INFILE("RANDOM_INCR");
	
	double T_FINAL = INFILE("T_FINAL");
	
	double R_FOLLOW = INFILE("R_FOLLOW");
	double R_TWEET = INFILE("R_TWEET");
	double R_ADD = INFILE("R_ADD");
	
	//#####################################################

	//DEFINE SOME VARIABLES NEEDED
	//#############################################################
	long double TIME = 0.0;    // the initial time is 0 minutes
	
	long double R_TOTAL =  R_ADD + R_FOLLOW*N_USERS + R_TWEET*N_USERS; 
	
	//Normalize the rates
	
	long double R_ADD_NORM = R_ADD / R_TOTAL;
	long double R_FOLLOW_NORM = R_FOLLOW * N_USERS  / R_TOTAL;
	long double R_TWEET_NORM = R_TWEET * N_USERS / R_TOTAL;
	//#############################################################
	
	/*FUTURE PLAN IDEAS
	//#####################################################################
	vector <char> PEOPLE_TYPE; // All we have right now is joes denoted as A
				 // A = joe, B = corporation, C = celeb --> future plans 
	for (int i = 0; i < N_JOES; i ++)
	{
		PEOPLE_TYPE.push_back('A');
	}
	for (int i = 0; i < N_CELEB; i ++)
	{
		PEOPLE_TYPE.push_back('C');
	}
	for (int i = 0; i < N_ORG; i ++)
	{
		PEOPLE_TYPE.push_back('B');
		
	}*/
	//#####################################################################
	
	//DECLARE THE MAIN NETWORK ARRAY
	//######################################################################
	long long int NETWORK[MAX_USERS][MAX_FOLLOWING]; // This is the main network array
	ofstream NETWORK_ARRAY;
	NETWORK_ARRAY.open("NETWORK_ARRAY");
	
	long long int NFOLLOWING[MAX_USERS];
	ofstream NFOLLOWING_DATA;
	NFOLLOWING_DATA.open("NFOLLOWING");
	
	// INITIALIZE THE ABOVE ARRAYS
	for (long long int i = 0; i < MAX_USERS; i ++)
	{
		NFOLLOWING[i] = 0;
		for (int j = 0; j < MAX_FOLLOWING; j ++)
		{
			NETWORK[i][j] = -1; // Used instead of 0, -1 shows no action in the array
		}
	}
	//#######################################################################

	//DECLARE OUTPUT FILE TO PLOT DATA
	//################################################
	ofstream DATA_TIME;
	DATA_TIME.open("DATA_vs_TIME");
	
	
	//###############################################

	//CHECK OUR NUMBER OF STEPS, TWEETS and FOLLOWS
	long long int NSTEPS = 0, N_TWEETS = 0, N_FOLLOWS = 0;

	//RANDOM SEED
	srand(time(NULL));
	
	while ( TIME < T_FINAL && N_USERS < MAX_USERS )
	{
		
			// get the first uniform number
                        // IT confirm that you have enough precision here
			 long double u_1 = ((unsigned long long) rand() % 1000000000 + 1) / 1000000000.0;	

			// If we find ourselves in the add user chuck of our cumuative function
			if (u_1 - R_ADD_NORM <= 0.0)
			{
				N_USERS ++;
				//call to function to decide which user to add
			}

			// If we find ourselves in the bond node chunk of our cumulative function
			else if (u_1 - (R_ADD_NORM + R_FOLLOW_NORM) <= 0.0 )
			{
				N_FOLLOWS ++;
				double val = u_1 - R_ADD_NORM;
				int user = val / (R_FOLLOW_NORM / N_USERS);  // this finds the user
				NETWORK[user][NFOLLOWING[user]] = rand() % N_USERS;
				NFOLLOWING[user] ++;
			}
			
			// if we find ourselves in the tweet chuck of the cumulative function
			else if (u_1 - (R_ADD_NORM + R_FOLLOW_NORM + R_TWEET_NORM) <= 1e-15)
			{
				N_TWEETS ++;
				double val = u_1 - R_ADD_NORM - R_FOLLOW_NORM;
				int user = val/(R_TWEET_NORM/N_USERS); // this finds the user
			}
			
                        else
                        {
                        cout << "Disaster, event out of bounds" << endl;
                        }
			// IT there should be different levels of verbosity. 0=debug, 1=normal, 2=no data writing at all
			if (OUTPUT == 0)
			{ DATA_TIME << fixed << setprecision(10) << "Increment: " << NSTEPS <<  "     Time: " << TIME << "     Random Number 1: " << u_1 << "     N_Users: " << N_USERS << "     N_Follows: " << N_FOLLOWS << "     N_Tweets: " << N_TWEETS << "     R_Add_Norm: " << R_ADD_NORM << "     R_Follow_Norm: " << R_FOLLOW_NORM << "     R_Tweet_Norm: " << R_TWEET_NORM << endl;
			}
			else if (OUTPUT == 1)
			{
				DATA_TIME << fixed << setprecision(5) << "TIME (min):" << "\t" << TIME << "\t" << "NUMBER OF USERS:" << "\t" << N_USERS << "\t" << "NUMBER OF FOLLOWS:" << "\t" << N_FOLLOWS << "\t" << "NUMBER OF TWEETS:" << "\t" << N_TWEETS << endl;
			}

			if (RANDOM_INCR == 1)
			{
				// get second random number
				double u_2 = (rand() % 1000 + 1) / 1000.0;
				
				// increment by random time
				TIME += -log(u_2)/R_TOTAL;
			}
			else
			{
				TIME += 1/R_TOTAL;
			}

			NSTEPS ++;
			
			//update the rates if n_users has changed
			R_TOTAL = R_ADD + R_FOLLOW * N_USERS + R_TWEET * N_USERS;
			R_ADD_NORM = R_ADD / R_TOTAL; 
			R_FOLLOW_NORM = R_FOLLOW * N_USERS / R_TOTAL;
			R_TWEET_NORM = R_TWEET * N_USERS / R_TOTAL;
			
	}
        // IT It looks like you may have issues with long vs short ints. NSTEPS went negative for one of my test cases. 
	
if (TIME < T_FINAL)
	{
		cout << "\nReached maximum number of users.\n\n- - Program Terminated - -\n\n";
	}
	else
	{
		cout << "\nReached maximum amount of time.\n\n- - Program Terminated - -\n\n";
	}
	DATA_TIME.close();
	int blah = 30000000;
	cout << blah << endl;
			
return 0;
}
