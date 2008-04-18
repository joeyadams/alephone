/*
 *  metaserver_dialogs.cpp - UI for metaserver client

	Copyright (C) 2004 and beyond by Woody Zenfell, III
	and the "Aleph One" developers.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

 April 29, 2004 (Woody Zenfell):
	Created.
 */

#if !defined(DISABLE_NETWORKING)

#include "cseries.h"

#include "alephversion.h"
#include "metaserver_dialogs.h"
#include "network_private.h" // GAME_PORT
#include "preferences.h"
#include "network_metaserver.h"
#include "map.h" // for _force_unique_teams!?!
#include "SoundManager.h"

#include "Update.h"
#include "progress.h"

extern ChatHistory gMetaserverChatHistory;
extern MetaserverClient* gMetaserverClient;

const IPaddress
run_network_metaserver_ui()
{
	return MetaserverClientUi::Create()->GetJoinAddressByRunning();
}


void
setupAndConnectClient(MetaserverClient& client)
{
	{
		unsigned char* playerNameCStringStorage = pstrdup(player_preferences->name);
		char* playerNameCString = a1_p2cstr(playerNameCStringStorage);
		client.setPlayerName(playerNameCString);
		free(playerNameCStringStorage);
	}

	// Check the updates URL for updates
	
	static bool user_informed = false;

	if (network_preferences->check_for_updates && !user_informed)
	{
		static bool first_check = true;
		
		if (first_check)
		{
			uint32 ticks = SDL_GetTicks();

			// if we get an update in a short amount of time, don't display progress
			while (Update::instance()->GetStatus() == Update::CheckingForUpdate && SDL_GetTicks() - ticks < 500);

			// check another couple seconds, but with a progress dialog
			open_progress_dialog(_checking_for_updates);
			while (Update::instance()->GetStatus() == Update::CheckingForUpdate && SDL_GetTicks() - ticks < 2500);
			close_progress_dialog();
			first_check = false;
		}

		Update::Status status = Update::instance()->GetStatus();
		if (status == Update::UpdateAvailable)
		{
			dialog d;

			d.add(new w_static_text("UPDATE AVAILABLE", TITLE_FONT, TITLE_COLOR));
			d.add(new w_spacer());

			d.add(new w_static_text("An update for Aleph One is available."));
			d.add(new w_static_text("Please download it from"));
			d.add(new w_static_text("http://marathon.sourceforge.net/"));
			d.add(new w_static_text("before playing games online."));
			
			d.add(new w_spacer());
			d.add(new w_button("OK", dialog_ok, &d));
			d.run();
		}
		
		if (status != Update::CheckingForUpdate) user_informed = true;
	}

	client.setPlayerTeamName("");
	client.connect("metaserver.lhowon.org", 6321, network_preferences->metaserver_login, network_preferences->metaserver_password);
}



GameAvailableMetaserverAnnouncer::GameAvailableMetaserverAnnouncer(const game_info& info)
{
	setupAndConnectClient(*gMetaserverClient);

	GameDescription description;
	description.m_type = info.net_game_type;
	description.m_timeLimit = info.time_limit == INT32_MAX ? -1 : info.time_limit;
	description.m_difficulty = info.difficulty_level;
	description.m_mapName = string(info.level_name);
	description.m_name = gMetaserverClient->playerName() + "'s Game";
	description.m_teamsAllowed = !(info.game_options & _force_unique_teams);
	
	// description's constructor gets scenario info, aleph one's protocol ID for us
	
	description.m_alephoneBuildString = string(A1_DISPLAY_VERSION) + " (" + A1_DISPLAY_PLATFORM + ")";
	
	if (network_preferences->use_netscript)
	{
#ifdef mac
		FileSpecifier netScript;
		netScript.SetSpec(network_preferences->netscript_file);
#else
		FileSpecifier netScript(network_preferences->netscript_file);
#endif
		char netScriptName[256];
		netScript.GetName(netScriptName);
		netScriptName[32] = '\0';
		description.m_netScript = netScriptName;
	} // else constructor's blank string is desirable
	
	gMetaserverClient->announceGame(GAME_PORT, description);
}

void GameAvailableMetaserverAnnouncer::Start(int32 time_limit)
{
	gMetaserverClient->announceGameStarted(time_limit);

}

extern void PlayInterfaceButtonSound(short);

void GlobalMetaserverChatNotificationAdapter::playersInRoomChanged(const std::vector<MetaserverPlayerInfo>& playerChanges)
{
	// print some notifications to the chat window
	for (size_t i = 0; i < playerChanges.size(); i++) 
	{
		if (playerChanges[i].verb() == MetaserverClient::PlayersInRoom::kAdd)
		{
			receivedLocalMessage(playerChanges[i].name() + " has joined the room");
		}
		else if (playerChanges[i].verb() == MetaserverClient::PlayersInRoom::kDelete)
		{
			receivedLocalMessage(playerChanges[i].name() + " has left the room");
		}
	}
}

void GlobalMetaserverChatNotificationAdapter::gamesInRoomChanged(const std::vector<GameListMessage::GameListEntry>& gameChanges)
{
	for (size_t i = 0; i < gameChanges.size(); i++) {
		if (gameChanges[i].verb() == MetaserverClient::GamesInRoom::kAdd) {
			string name;
			// find the player's name
			for (size_t playerIndex = 0; playerIndex < gMetaserverClient->playersInRoom().size(); playerIndex++)
			{
				if (gMetaserverClient->playersInRoom()[playerIndex].id() == gameChanges[i].m_hostPlayerID) {
					name = gMetaserverClient->playersInRoom()[playerIndex].name();
					break;
				}
				}
			
			if (name.size() > 0) {
				receivedLocalMessage(gameChanges[i].format_for_chat(name));
			}
		}
	}
}

void GlobalMetaserverChatNotificationAdapter::receivedChatMessage(const std::string& senderName, uint32 senderID, const std::string& message)
{
	gMetaserverChatHistory.appendString(senderName + ": " + message);
	PlayInterfaceButtonSound(_snd_computer_interface_logon);
}

void GlobalMetaserverChatNotificationAdapter::receivedPrivateMessage(const std::string& senderName, uint32 senderID, const std::string& message)
{
	gMetaserverChatHistory.appendString(senderName + "> " + message);
}

void GlobalMetaserverChatNotificationAdapter::receivedBroadcastMessage(const std::string& message)
{
	gMetaserverChatHistory.appendString("@ " + message + " @");
}

void GlobalMetaserverChatNotificationAdapter::receivedLocalMessage(const std::string& message)
{
	gMetaserverChatHistory.appendString("( " + message + " )");
}

void MetaserverClientUi::delete_widgets ()
{
	delete m_playersInRoomWidget;
	delete m_gamesInRoomWidget;
	delete m_chatEntryWidget;
	delete m_textboxWidget;
	delete m_cancelWidget;
}

const IPaddress MetaserverClientUi::GetJoinAddressByRunning()
{
	// This was designed with one-shot-ness in mind
	assert(!m_used);
	m_used = true;
	
	obj_clear(m_joinAddress);

	setupAndConnectClient(*gMetaserverClient);
	gMetaserverClient->associateNotificationAdapter(this);

	m_gamesInRoomWidget->SetItemSelectedCallback(bind(&MetaserverClientUi::GameSelected, this, _1));
	m_playersInRoomWidget->SetItemSelectedCallback(bind(&MetaserverClientUi::PlayerSelected, this, _1));
	m_muteWidget->set_callback(boost::bind(&MetaserverClientUi::MuteClicked, this));
	m_chatEntryWidget->set_callback(bind(&MetaserverClientUi::ChatTextEntered, this, _1));
	m_cancelWidget->set_callback(boost::bind(&MetaserverClientUi::handleCancel, this));

	gMetaserverChatHistory.clear ();
	m_textboxWidget->attachHistory (&gMetaserverChatHistory);

	if (Run() < 0) {
		handleCancel();
	}
	
	return m_joinAddress;
}

void MetaserverClientUi::GameSelected(GameListMessage::GameListEntry game)
{
	// check if the game is compatible...a more elegant dialog should
	// appear here in the future, but for now, just prevent joining 
	// incompatible games

	if (game.running()) 
	{
		string message = game.name();
		message += " is in progress";
		int minutes_remaining = game.minutes_remaining();
		if (minutes_remaining == 0)
		{
			message += ", about to end";
		}
		else if (minutes_remaining == 1)
		{
			message += ", approximately 1 minute remaining";
		}
		else if (minutes_remaining > 0)
		{
			char minutes[5];
			snprintf(minutes, 4, "%i", minutes_remaining);
			minutes[4] = '\0';
			message += ", approximately ";
			message += minutes;
			message += " minutes remaining";
		}
		receivedLocalMessage(message);
		return;
	}

	if (!Scenario::instance()->IsCompatible(game.m_description.m_scenarioID))
	{
		string joinersScenario = (Scenario::instance()->GetName() != "") ? (Scenario::instance()->GetName()) : (Scenario::instance()->GetID());
		string gatherersScenario = (game.m_description.m_scenarioName != "") ? game.m_description.m_scenarioName : game.m_description.m_scenarioID;
		string errorMessage = joinersScenario + " is unable to join " + gatherersScenario + " games. Please restart using " + gatherersScenario + " to join this game.";

		alert_user(const_cast<char *>(errorMessage.c_str()), 0);
		return;
	}

	memcpy(&m_joinAddress.host, &game.m_ipAddress, sizeof(m_joinAddress.host));
	m_joinAddress.port = game.m_port;
	Stop();
}

void MetaserverClientUi::UpdatePlayerButtons()
{
	if (gMetaserverClient->player_target() == MetaserverPlayerInfo::IdNone)
	{
		m_muteWidget->deactivate();
	}
	else
	{
		m_muteWidget->activate();
	}	
}

void MetaserverClientUi::PlayerSelected(MetaserverPlayerInfo info)
{
	if (gMetaserverClient->player_target() == info.id())
	{
		gMetaserverClient->player_target(MetaserverPlayerInfo::IdNone);
	}
	else
	{
		gMetaserverClient->player_target(info.id());
	}

	std::vector<MetaserverPlayerInfo> sortedPlayers = gMetaserverClient->playersInRoom();
	std::sort(sortedPlayers.begin(), sortedPlayers.end(), MetaserverPlayerInfo::sort);

	m_playersInRoomWidget->SetItems(sortedPlayers);
	UpdatePlayerButtons();
}

void MetaserverClientUi::MuteClicked()
{
	gMetaserverClient->ignore(gMetaserverClient->player_target());
}

void MetaserverClientUi::playersInRoomChanged(const std::vector<MetaserverPlayerInfo> &playerChanges)
{
	std::vector<MetaserverPlayerInfo> sortedPlayers = gMetaserverClient->playersInRoom();
	std::sort(sortedPlayers.begin(), sortedPlayers.end(), MetaserverPlayerInfo::sort);

	m_playersInRoomWidget->SetItems(sortedPlayers);
	UpdatePlayerButtons();
	GlobalMetaserverChatNotificationAdapter::playersInRoomChanged(playerChanges);

}

void MetaserverClientUi::gamesInRoomChanged(const std::vector<GameListMessage::GameListEntry> &gameChanges)
{
	std::vector<GameListMessage::GameListEntry> sortedGames = gMetaserverClient->gamesInRoom();
	std::sort(sortedGames.begin(), sortedGames.end(), GameListMessage::GameListEntry::sort);
	m_gamesInRoomWidget->SetItems(sortedGames);
	GlobalMetaserverChatNotificationAdapter::gamesInRoomChanged(gameChanges);
	for (size_t i = 0; i < gameChanges.size(); i++) 
	{
		if (gameChanges[i].verb() == MetaserverClient::GamesInRoom::kAdd && !gameChanges[i].running())
		{
			PlayInterfaceButtonSound(_snd_got_ball);
			break;
		}
	}
}

void MetaserverClientUi::sendChat()
{
	string message = m_chatEntryWidget->get_text();
	if (gMetaserverClient->player_target() != MetaserverPlayerInfo::IdNone)
	{
		gMetaserverClient->sendPrivateMessage(gMetaserverClient->player_target(), message);
		gMetaserverClient->player_target(MetaserverPlayerInfo::IdNone);
		std::vector<MetaserverPlayerInfo> sortedPlayers = gMetaserverClient->playersInRoom();
		std::sort(sortedPlayers.begin(), sortedPlayers.end(), MetaserverPlayerInfo::sort);
		
		m_playersInRoomWidget->SetItems(sortedPlayers);
		UpdatePlayerButtons();
	}
	else
		gMetaserverClient->sendChatMessage(message);
	m_chatEntryWidget->set_text(string());
}
	
void MetaserverClientUi::ChatTextEntered (char character)
{
	if (character == '\r')
		sendChat();
}

void MetaserverClientUi::handleCancel ()
{
	// gMetaserverClient->disconnect ();
	delete gMetaserverClient;
	gMetaserverClient = new MetaserverClient ();
	Stop ();
}

#endif // !defined(DISABLE_NETWORKING)
