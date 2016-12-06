#include "JoystickPool.h"

#include <iostream>
#include <cstdio>
#include <cstring>
#include <sstream>

#include <SDL2/SDL.h>

// Joystick Pool
JoystickPool* JoystickPool::mSingleton = 0; //static

JoystickPool& JoystickPool::getSingleton()
{
	if (mSingleton == 0)
		mSingleton = new JoystickPool();

	return *mSingleton;
}

JoyPad JoystickPool::getJoystick(int id)
{
	JoyPad jp =  mJoyMap[id];

	if (!jp.joy && !jp.pad)
	{
		std::cerr << "Warning: could not find joystick number " << id << "!" << std::endl;
		mJoyMap.erase(id);
	}

	return jp;
}

void JoystickPool::probeJoysticks()
{
	/*
	int numJoysticks = SDL_NumJoysticks();
	SDL_Joystick* lastjoy;
	for(int i = 0; i < numJoysticks; i++)
	{
		lastjoy = SDL_JoystickOpen(i);

		if (lastjoy == NULL)
			continue;

		mJoyMap[SDL_JoystickInstanceID(lastjoy)] = lastjoy;
	}
	*/
}

void JoystickPool::openJoystick(const int joyIndex)
{
	JoyPad jp = {NULL, NULL};

	if (SDL_IsGameController(joyIndex))
	{
		jp.pad = SDL_GameControllerOpen(joyIndex);
		if (jp.pad != NULL)
		{
			mJoyMap[SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(jp.pad))] = jp;
		}
	}
	else
	{
		jp.joy = SDL_JoystickOpen(joyIndex);
		if (jp.joy != NULL)
		{
			mJoyMap[SDL_JoystickInstanceID(jp.joy)] = jp;
		}
	}
}

void JoystickPool::closeJoystick(const int joyId)
{
	JoyPad jp = mJoyMap[joyId];
	if (jp.joy != NULL)
	{
		SDL_JoystickClose(jp.joy);
	}
	if (jp.pad != NULL)
	{
		SDL_GameControllerClose(jp.pad);
	}
	mJoyMap.erase(joyId);
}

void JoystickPool::closeJoysticks()
{
	for (JoyMap::iterator iter = mJoyMap.begin();
		iter != mJoyMap.end(); ++iter)
	{
		closeJoystick((*iter).first);
	}
}

// Joystick Action

JoystickAction::JoystickAction(std::string string)
{
	type = AXIS;
	number = 0;
	joyPad = {NULL, NULL};
	joyid = 0;
	try
	{
		const char* str = string.c_str();
		if (std::strstr(str, "button"))
		{
			type = BUTTON;
			if (sscanf(str, "joy_%d_button_%d", &joyid, &number) < 2)
				throw string;
		}
		else if (std::strstr(str, "axis"))
		{
			if (sscanf(str, "joy_%d_axis_%d", &joyid, &number) < 2)
				throw string;
		}
		else if (std::strstr(str, "gamepad"))
		{
			if (sscanf(str, "gamepad_%d", &joyid) < 1)
				throw string;
			if (std::strstr(str, "left"))
			{
				number = 0;
			}
			else if (std::strstr(str, "right"))
			{
				number = 1;
			}
			else
			{
				number = 2;
			}
		}

		joyPad = JoystickPool::getSingleton().getJoystick(joyid);
	}
	catch (const std::string& e)
	{
		std::cerr << "Parse error in joystick config: " << e << std::endl;
	}
}

JoystickAction::JoystickAction(const JoystickAction& action)
{
	type = action.type;
	joyPad = JoystickPool::getSingleton().getJoystick(action.joyid);
	joyid = action.joyid;
	number = action.number;
}

JoystickAction::~JoystickAction()
{
}

std::string JoystickAction::toString()
{
	const char* typestr = "unknown";
	if (type == AXIS)
	{
		typestr = "axis";
	}

	if (type == BUTTON)
	{
		typestr = "button";
	}

	std::stringstream buf;
	buf << "joy_" << joyid << "_" << typestr << "_" << number;
	return buf.str();
}

KeyAction JoystickAction::toKeyAction()
{
	// This is an place holder
	// Just to get simple gamepad support for the GUI
	// TODO: Fix that! Use GUID -> ButtonMapping to get this think cleaned up
	if (type == AXIS)
	{
		// X-Achse?!
		if (abs(number) == 1)
		{

			if (number < 0)
			{
				return KeyAction::LEFT;
			}
			else
			{
				return KeyAction::RIGHT;
			}
		}

		// Y-Achse?!
		if (abs(number) == 2)
		{

			if (number < 0)
			{
				return KeyAction::UP;
			}
			else
			{
				return KeyAction::DOWN;
			}
		}
	}

	if (type == BUTTON)
	{
		if(number == 0)
		{
			return KeyAction::SELECT;
		}
		if(number == 1)
		{
			// TODO: This is stupid, we must check if this button is used ingame so that
			// the jumpbutton does not pause the game
			//return JoystickEvent::BACK;
			// We don't allow back at the moment -> game is not leavable
			return KeyAction::NONE;
		}
	}
	return KeyAction::NONE;
}
