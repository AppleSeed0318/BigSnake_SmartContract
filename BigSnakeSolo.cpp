#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <iterator>

#include <cstdlib>
#include <stdlib.h>
#include <ctime>

using namespace eosio;
using namespace std;

#define current_time eosio::current_time_point()

class[[eosio::contract("BigSnakeSolo")]] BigSnakeSolo : public eosio::contract {
	public:
		BigSnakeSolo(name receiver, name code,  datastream<const char*> ds): contract(receiver, code, ds),
		t_bsi("BSI", 0),
		t_bsm("BSM", 0),
		t_bsg("BSG", 0) {}
		// static constexpr symbol BSI_SYMBOL = symbol(symbol_code("BSI"), 4);
		// static constexpr symbol BSM_SYMBOL = symbol(symbol_code("BSM"), 4);
		// static constexpr symbol BSG_SYMBOL = symbol(symbol_code("BSG"), 4);

		// eosio::asset bsi, bsm, bsg;
		// bsi.symbol = BSI_SYMBOL;
		// bsi.amount = 0;
		// bsm.symbol = BSM_SYMBOL;
		// bsm.amount = 0;
		// bsg.symbol = BSG_SYMBOL;
		// bsg.amount = 0;
		
		// asset bsi = asset(0, t_bsi);
		// asset bsm = asset(0, t_bsm);
		// asset bsg = asset(0, t_bsg);
		
		
		// add person into person table
		[[eosio::action]]
		void addperson(name user) {
		
			require_auth(user);
			
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user.value);
			check(iterator == people.end(), "User already exist");
			if(iterator == people.end()) {
				people.emplace(user, [&](auto& row) {
					row.user = user;
					row.balance_BSI = asset(0, symbol("BSI", 4));
					row.balance_BSM = asset(0, symbol("BSI", 4));
					row.balance_BSG = asset(0, symbol("BSI", 4));
					row.level = 1;
					row.solo_killed = 0;
					row.respawn_time = current_time_point();
					row.health_point = 50;
				});
			}
		}
		
		// remove person from person table
		[[eosio::action]]
		void removeperson(name user) {
			require_auth(user);
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user.value);
			check(iterator != people.end(), "User does not exist");
			people.erase(iterator);
			
		}
		
		// update BSI, BSM, BSG token balances
		[[eosio::action]]
		void updatebalance(name user, uint64_t v_bsi, uint64_t v_bsm, uint64_t v_bsg) {
			require_auth(user);
			
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user.value);
			
			if(iterator != people.end()) {
				people.modify(iterator, user, [&](auto& row) {
					row.balance_BSI.amount += v_bsi;
					row.balance_BSM.amount += v_bsm;
					row.balance_BSG.amount += v_bsg;
				});
			}
		}
		
		// lost 1 BSG and alive
		// return true: success,  false: not enough BSG token
		[[eosio::action]]
		bool fastrespawn(name user) {
			require_auth(user);
			
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user.value);
			
			check(iterator != people.end(), "check user");
			if(iterator->balance_BSG.amount > 0) {
				people.modify(iterator, user, [&](auto& row) {
					row.respawn_time = current_time;
					row.balance_BSG.amount -= 1;
				});
				return true;
			}
			return false;
		}
		
		[[eosio::action]]
		bool eatanddrink(name user) {
			require_auth(user);
			
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user.value);
			
			check(iterator != people.end(), "check user");
			if(iterator->balance_BSG.amount > 0) {
				people.modify(iterator, user, [&](auto& row) {
					row.health_point = 100;
					row.balance_BSG.amount -= 1;
				});
				return true;
			}
			return false;
		}

		// check win or loss
		// return 4: lose, 5: wait until respawn, 6: not enough health
		[[eosio::action]]
		int checksolowin(name user) {
			
			require_auth(user);
			
			person_index people(get_self(), get_first_receiver().value);
			auto iterator = people.find(user.value);
			check(iterator != people.end(), "check user");
			
			// win check logic

			uint32_t rand = current_time.sec_since_epoch();

			if ((rand % 3) % 2 == 0) {
				uint32_t respawn_period = 1 * 60 * 60;
				time_point_sec cur = current_time;

				people.modify(iterator, user, [&](auto& row) {
					row.respawn_time = cur + respawn_period;
					row.solo_killed = 0;
					row.health_point = 0;
				});
				return 4;
			}
			
			//check respawn time
			if(iterator->respawn_time > current_time) {
				return 5;
			}
			
			//not enough health
			if( iterator->health_point < 5 * (iterator->level * 3 - 2) ) {
				return 6;
			}
			
			// increase BSM token or killed snake count
			if(iterator->solo_killed == iterator->level*3 - 2) {
				people.modify(iterator, user, [&](auto& row) {
					row.level += 1;
					row.solo_killed = 0;
					row.balance_BSM.amount += iterator->level*3 - 2;
				});
				// remove contract BSM balance in owner balance
				person_index people(get_self(), get_first_receiver().value);
				auto iterator_o = people.find(get_self().value);
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
			stakelist_index stakes(get_self(), get_first_receiver().value);
			check(nfts.size()>0, "no stake data");
			
			auto iterator = stakes.find(user.value);
			
			time_point_sec cur = current_time;
			if(iterator != stakes.end()) { // not exist yet
				stakes.emplace(user, [&](auto& row) {
					row.user = user;
					row.nfts = nfts;
					row.t_start = cur;
					row.t_end = cur + stake_period;
					row.t_update = cur;
				});
			}
			else { // find exist one
				stakes.modify(iterator, user, [&](auto& row){
					row.t_update = cur;
					for (auto nft : nfts) row.nfts.push_back(nft);
				});
			}
			

			//check rewarded user
			for (auto it : stakes) {
				time_point_sec cur = current_time;

				if(it.t_end >= cur) {
					
					// set default values  --- stake start
					stakes.modify(iterator, user, [&](auto& row){
						row.t_update = cur;
						row.t_start = cur;
						row.t_end = cur + stake_period;
					});
					
					// increase BSG token
					person_index people(get_self(), get_first_receiver().value);
					auto iterator_p = people.find(user.value);
					check(iterator_p != people.end(), "User does not exist");
					
					people.modify(iterator_p, user, [&](auto& row) {
						row.balance_BSG.amount += 1;
					});
					
					// remove contract BSG balance in owner balance
					auto iterator_o = people.find(get_self().value);
					check(iterator_o != people.end(), "Owner does not exist");
					
					people.modify(iterator_o, user, [&](auto& row) {
						row.balance_BSG.amount -= 1;
					});
				}
			}
		}
		
	private:

		const symbol t_bsi, t_bsm, t_bsg;
		const uint32_t stake_period = 4 * 60 * 60;

		TABLE person {
			name user;
			
			asset balance_BSI, balance_BSM, balance_BSG;
			
			//for solo
			int solo_killed;
			int level;
			time_point_sec respawn_time;
			int health_point;
			
			person(){
				balance_BSI = asset(0, symbol("BSI", 4));
				balance_BSM = asset(0, symbol("BSM", 4));
				balance_BSG = asset(0, symbol("BSG", 4));
				solo_killed = 0;
				health_point = 50;
				respawn_time = current_time_point();
				level = 1;
			}
			
			uint64_t primary_key() const { return user.value; }
		};
		using person_index = eosio::multi_index<"people"_n, person>;
		
		TABLE stakelist {
			name user;
			vector<uint64_t> nfts;
			time_point_sec t_start;
			time_point_sec t_end;
			time_point_sec t_update;
			
			uint64_t primary_key() const { return user.value; }
		};
		
		using stakelist_index = eosio::multi_index<"stakelists"_n, stakelist>;
};

