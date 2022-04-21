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
	
};

