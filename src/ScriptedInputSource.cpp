/*=============================================================================
Blobby Volley 2
Copyright (C) 2006 Jonathan Sieber (jonathan_sieber@yahoo.de)
Copyright (C) 2006 Daniel Knobe (daniel-knobe@web.de)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
=============================================================================*/

/* header include */
#include "ScriptedInputSource.h"

/* includes */
#include <cmath>
#include <algorithm>
#include <iostream>

#include <SDL2/SDL.h>

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}

#include "DuelMatch.h"
#include "DuelMatchState.h"
#include "GameConstants.h"

/* implementation */
ScriptedInputSource* ScriptedInputSource::mCurrentSource = 0;

ScriptedInputSource::ScriptedInputSource(const std::string& filename, PlayerSide playerside, unsigned int difficulty)
: mMaxDelay(difficulty)
, mLastBallSpeed(0)
, mSide(playerside)
{
	mStartTime = SDL_GetTicks();

	// set game constants
	setGameConstants();
	setGameFunctions();

	lua_register(mState, "jump", jump);
	lua_register(mState, "left", left);
	lua_register(mState, "right", right);

	openScript("api");
	openScript("bot_api");
	openScript(filename);

	// check whether all required lua functions are available
	bool onserve, ongame, onoppserve;
	onserve = getLuaFunction("OnServe");
	ongame = getLuaFunction("OnGame");
	onoppserve = getLuaFunction("OnOpponentServe");
	if (!onserve || !ongame ||!onoppserve)
	{
		std::string error_message = "Missing bot function ";
		error_message += onserve ? "" : "OnServe() ";
		error_message += ongame ? "" : "OnGame() ";
		error_message += onoppserve ? "" : "OnOpponentServe() ";
		std::cerr << "Lua Error: " << error_message << std::endl;

		ScriptException except;
		except.luaerror = error_message;
		BOOST_THROW_EXCEPTION(except);
	}

	// record which of the optional functions are available
	mOnBounce = getLuaFunction("OnBounce");

	if(!mOnBounce)
	{
		std::cerr << "Lua Warning: Missing function OnBounce" << std::endl;
	}

	// clean up stack
	lua_pop(mState, lua_gettop(mState));
}

ScriptedInputSource::~ScriptedInputSource()
{
}

PlayerInputAbs ScriptedInputSource::getNextInput()
{
	bool serving = false;
	// reset input
	mLeft = false;
	mRight = false;
	mJump = false;

	mCurrentSource = this;
	if (getMatch() == 0)
	{
		return PlayerInputAbs();
	}

	// collect match states
	if(mSide == LEFT_PLAYER)
	{
		updateGameState(getMatch()->getState());
	}
	else
	{
		auto s = getMatch()->getState();
		s.swapSides();
		updateGameState( s );
	}

	// detect bounce
	if (mOnBounce && mLastBallSpeed != getMatch()->getBallVelocity().x )
	{
		mLastBallSpeed = getMatch()->getBallVelocity().x;
		lua_getglobal(mState, "OnBounce");
		callLuaFunction();
	}

	if (!getMatch()->getBallActive() && mSide ==
			// if no player is serving player, assume the left one is
			(getMatch()->getServingPlayer() == NO_PLAYER ? LEFT_PLAYER : getMatch()->getServingPlayer() ))
	{
		serving = true;
		lua_getglobal(mState, "OnServe");
		lua_pushboolean(mState, !getMatch()->getBallDown());
		callLuaFunction(1);
	}
	else if (!getMatch()->getBallActive() && mCurrentSource->mSide !=
			(getMatch()->getServingPlayer() == NO_PLAYER ? LEFT_PLAYER : getMatch()->getServingPlayer() ))
	{
		lua_getglobal(mState, "OnOpponentServe");
		callLuaFunction();
	}
	else
	{
		lua_getglobal(mState, "OnGame");
		callLuaFunction();
	}

	// swap left/right if side is swapped
	if ( mSide == RIGHT_PLAYER )
		std::swap(mLeft, mRight);

	int stacksize = lua_gettop(mState);
	if (stacksize > 0)
	{
		std::cerr << "Warning: Stack messed up!" << std::endl;
		std::cerr << "Element on stack is a ";
		std::cerr << lua_typename(mState, -1) << std::endl;
		lua_pop(mState, stacksize);
	}

	if (mStartTime + WAITING_TIME > SDL_GetTicks() && serving)
		return PlayerInputAbs();
	else
		return PlayerInputAbs(mLeft, mRight, mJump);
}

int ScriptedInputSource::jump(lua_State* state)
{
	mCurrentSource->mJump = true;
	return 0;
}

int ScriptedInputSource::left(lua_State* state)
{
	mCurrentSource->mLeft = true;
	return 0;
}

int ScriptedInputSource::right(lua_State* state)
{
	mCurrentSource->mRight = true;
	return 0;
}
