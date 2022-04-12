#include <eosio/eosio.hpp>

using namespace eosio;
using namespace std;

#define pb push_back
#define stake_period 4*60*60

class[[eosio::contract("solo")]] solo : public eosio::contract {
	public:
		solo(name receiver, name code,  datastream<const char*> ds): contract(receiver, code, ds) {}
		
		asset BSI.symbol = "BSI";
		asset BSI.amount = 0.0000;
		asset BSM.symbol = "BSM";
		asset BSM.amount = 0.0000;
		asset BSG.symbol = "BSG";
		asset BSG.amount = 0.0000;
		
		// add person into person table
		[[eosio::action]]
		void add_person(name user) {
		
			require_auth(user);
			
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user);
			check(iterator == people.end(), "User already exist");
			if(iterator == people.end()) {
				people.emplace(user, [&](auto& row) {
					row.user = user;
					row.balance_BSI = BSI;
					row.balance_BSM = BSM;
					row.balance_BSG = BSG;
					row.level = 1;
					row.solo_killed = 0
					row.respawn_time = 0;
					row.health_point = 50;
				});
			}
		}
		
		// remove person from person table
		[[eosio::action]]
		void remove_person(name user) {
			require_auth(user);
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user);
			check(iterator != people.end(), "User does not exist");
			people.erase(iterator);
			
		}
		
		// update BSI, BSM, BSG token balances
		[[eosio::action]]
		void update_balance(name user, uint64_t bsi, uint64_t bsm, uint64_t bsg) {
			require_auth(user);
			
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user);
			
			if(iterator != people.end()) {
				people.modify(iterator, user, [&](auto& row) {
					row.balance_BSI.amount += bsi;
					row.balance_BSM.amount += bsm;
					row.balance_BSG.amount += bsg;
				});
			}
		}
		
		// lost 1 BSG and alive
		// return true: success,  false: not enough BSG token
		[[eosio::action]]
		bool fastRespawn(name user) {
			require_auth(user);
			
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user);
			
			check(iterator != people.end(), "check user");
			if(iterator.balance_BSG.amount > 0) 
				people.modify(iterator, user, [&](auto& row) {
					row.respawn_time = time(0);
					row.health_point += 20;
					row.balance_BSG.amount -= 1;
				});
				return true;
			}
			return false;
		}
		
		// check win or loss
		// return 4: lose, 5: wait until respawn, 6: not enough health
		[[eosio::action]]
		int checkSoloWin(name user) {
			
			require_auth(user);
			
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user);
			check(iterator != people.end(), "check user");
			
			// win check logic
			srand(time(0));
			if (rand()%3 % 2 == 0) {
				
				people.modify(iterator, user, [&](auto& row) {
					row.respawn_time = time(0) + 1*60*60;
					row.solo_killed = 0;
					row.health_point = 0;
				});
				return 4;
			}
			
			//check respawn time
			if(iterator.respawn_time > time(0)) {
				return 5;
			}
			
			//not enough health
			if( iterator.health_point < 5 * (level * 3 - 2) ) {
				return 6;
			}
			
			// increase BSM token or killed snake count
			if(iterator.solo_killed == iterator.level*3 - 2) {
				people.modify(iterator, user, [&](auto& row) {
					row.level += 1;
					row.solo_killed = 0;
					balance_BSM.amount += iterator.level*3 - 2;
					row.health_point += 20;
				});
				// remove contract BSM balance in owner balance
				person_index people(get_self(), get_first_receiver().value);
				auto iterator_o = people.find(get_self());
				check(iterator_o != people.end(), "Owner does not exist");
				
				people.modify(iterator_o, user, [&](auto& row) {
					row.balance_BSM.amount -= 1;
				});
			}
			else {
				people.modify(iterator, user, [&](auto& row) {
					row.solo_killed += 1;
					row.health_point -= 5;
				});
			}
			
			return true;
		}
		
		[[eosio::action]]
		void stake(name user, vector<uint64_t> nfts, string msg) {
			require_auth(user);
			stakeList_index = stakes(get_self(), get_first_receiver().value);
			check(nfts.size()>0, "no stake data");
			
			auto iterator = stakes.find(user);
			
			if(iterator != stakes.end()) { // not exist yet
				stakes.emplace(user, [&](auto& row) {
					row.user = user;
					row.nfts = nfts;
					row.t_start = time(0);
					row.t_end = time(0) + stake_period;
					row.t_update = time(0);
				});
			}
			else { // find exist one
				stakes.modify(iterator, user, [&](auto& row){
					row.t_update = time(0);
					auto (nft : nfts) row.nfts.pb(nft);
				});
			}
			
			//check rewarded user
			for (auto it : stakes) {
				if(it.t_end >= time(0)) {
					
					// set default values  --- stake start
					stakes.modify(iterator, user, [&](auto& row){
						row.t_update = time(0);
						row.t_start = time(0);
						row.t_end = time(0) + stake_period;
					});
					
					// increase BSG token
					person_index people(get_self(), get_first_receiver().value);
					auto iterator_p = people.find(user);
					check(iterator_p != people.end(), "User does not exist");
					
					people.modify(iterator_p, user, [&](auto& row) {
						row.balance_BSG.amount += 1;
					});
					
					// remove contract BSG balance in owner balance
					person_index people(get_self(), get_first_receiver().value);
					auto iterator_o = people.find(get_self());
					check(iterator_o != people.end(), "Owner does not exist");
					
					people.modify(iterator_o, user, [&](auto& row) {
						row.balance_BSG.amount -= 1;
					});
				}
			}
		}
		
	private:
		struct [[eosio::table]] person {
			name user;
			
			asset balance_BSI, balance_BSM, balance_BSG;
			
			//for solo
			int solo_killed;
			int level;
			int respawn_time;
			
			person(){
				balance_BSI = BSI;
				balance_BSM = BSM;
				balance_BSG = BSG;
				solo_killed = 0;
				health_point = 50;
				respawn_time = 0;
				level = 1;
			}
			
			uint64_t primary_key() const { return user.value; }
		};
		using person_index = eosio::multi_index<"people"_n, person>;
		
		struct [[eosio:table]] stakeList {
			name user;
			vector<asset> nfts;
			time t_start;
			time t_end;
			time t_update;
			
			uint64_t primary_key() const { return user.value; }
		}
		
		using stakeList_index = eosio::multi_index<"stakeLists"_n, stakeList>;
}
