#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"
#include "Walnut/Image.h"

#include <string>
#include <iostream>
#include <filesystem>
#include <dpp/dpp.h>
#include <iomanip>
#include <sstream>
 
#include <vector>
#include <fstream>
#include <iostream>
#include <mpg123.h>
#include <out123.h>


#include <atomic>
#include <csignal>
#include <iostream>
#include <stop_token>
#include <thread>
#include "async/awaitable_get.h"
#include "async/task.h"

#pragma execution_character_set("utf-8")

constexpr uint_fast16_t SampleRate = 48000;

class BotLayer : public Walnut::Layer
{
public:
	virtual void OnUIRender() override
	{
		ImGui::Begin("Songs");
		if(ImGui::Button(".."))
		{
			GoUpInDirectoryView();
		}
		for(auto songPath : songPaths)
		{
			if(ImGui::Button(songPath.c_str()))
			{
				BrowseFileOrFolder(songPath.c_str());
				break;
			}
		}
		ImGui::End();
		ImGui::Begin("Media Controls");
		if(voiceConnection && voiceConnection->voiceclient->is_playing())
		{
			ImGui::Text(CurrentlyPlayingSongName.c_str());
			ImGui::Text(GetCurrentSongTime().c_str());
			ImGui::SameLine();
			ImGui::Text(CurrentSongLengthString.data());

			if(ImGui::Button("StopSong"))
			{
				StopSong();
			}
			ImGui::SameLine();
			if(ImGui::Button("Pause/Unpause"))
			{
				PauseOrUnpauseSong();
			}
		}
		
		ImGui::End();
	}

	void OnAttach() override;
	void OnDetach() override;
	void BrowseFileOrFolder(std::string Path);
	void GoUpInDirectoryView();

	async::task<void> LoadAndPlaySong(const std::string& songPath);
	void RefreshFolderView();
	void PlaySong(std::vector<uint8_t>& songData);
	void StopSong();
	void PauseOrUnpauseSong();

	std::string GetSongDuration(const time_t& fullSeconds);
	std::string GetCurrentSongTime();

	inline async::task<void> fire_and_forget()
	{
		
		co_return;
	}

private:
	std::vector<std::string> songPaths;
	dpp::cluster* bot = nullptr;
	dpp::snowflake guildId;
	dpp::voiceconn* voiceConnection = nullptr;
	dpp::discord_client* discordClient = nullptr;

	long SongLengthInSamples = 0;
	double SongLengthInSeconds = 0;

	std::string CurrentSongLengthString;
	std::string PathToSongFolder = "C://BotSongs/";
	std::string CurrentlyPlayingSongName;

	
};

async::task<void> BotLayer::LoadAndPlaySong(const std::string& songPath)
{
	std::vector<uint8_t> SongData;
	SongData.reserve(50000000);
	
	mpg123_init();
	int err = 0;
	unsigned char* buffer;
	size_t buffer_size, done;
	int channels, encoding;
	long rate;
 
	/* Note it is important to force the frequency to 48000 for Discord compatibility */
	mpg123_handle *mh = mpg123_new(NULL, &err);
	mpg123_param(mh, MPG123_FORCE_RATE, SampleRate, SampleRate);
 
	/* Decode entire file into a vector. You could do this on the fly, but if you do that
	* you may get timing issues if your CPU is busy at the time and you are streaming to
	* a lot of channels/guilds.
	*/
	
	buffer_size = mpg123_outblock(mh);
	buffer = new unsigned char[buffer_size];
 
	/* Note: In a real world bot, this should have some error logging */
	mpg123_open(mh, songPath.c_str());
	mpg123_scan(mh);
	mpg123_getformat(mh, &rate, &channels, &encoding);

	SongLengthInSamples = mpg123_length(mh);
	SongLengthInSeconds = static_cast<double>(SongLengthInSamples) / SampleRate;

	CurrentSongLengthString = GetSongDuration(SongLengthInSeconds);
	CurrentlyPlayingSongName = songPath.substr(songPath.find_last_of("/\\") + 1);


	unsigned int counter = 0;
	for (int totalBytes = 0; mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK; ) {
		for (size_t i = 0; i < buffer_size; i++) {
			SongData.emplace_back(buffer[i]);
		}
		counter += buffer_size;
		totalBytes += done;
	}
	delete[] buffer;
	mpg123_close(mh);
	mpg123_delete(mh);
	/* Clean up */
	mpg123_exit();

	PlaySong(SongData);
	co_return;
}

void BotLayer::RefreshFolderView()
{
	songPaths.clear();
	namespace fs = std::filesystem;
	for ( auto & entry : fs::directory_iterator(PathToSongFolder.c_str()))
	{
		std::string path = entry.path().string();
		std::replace( path.begin(), path.end(), '\\', '/'); 
		songPaths.emplace_back(path);
		std::cout << entry.path() << std::endl;
	}
}

void BotLayer::OnAttach()
{
	RefreshFolderView();
	//ImGuiIO& io = ImGui::GetIO();
	//io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 13, nullptr, io.Fonts->GetGlyphRangesDefault());
	/* Setup the bot */
	std::ifstream tokenFile;
	std::string token;
	std::string line;
	std::string FullPath = std::filesystem::current_path().string() + "\\token.txt";
	tokenFile.open(FullPath);
	if (tokenFile.is_open())
	{
		if(getline(tokenFile, line)) {
			token = line;
		}
		else
		{
			std::cout << "Token is empty";
			return;
		}
		tokenFile.close();
	}
    bot = new dpp::cluster(token);
    //bot->on_log(dpp::utility::cout_logger());

	
    /* The event is fired when someone issues your commands */
    bot->on_slashcommand([this](const dpp::slashcommand_t& event) {
        /* Check which command they ran */
        if (event.command.get_command_name() == "join") {
            /* Get the guild */
            dpp::guild* g = dpp::find_guild(event.command.guild_id);
        	guildId = event.command.guild_id;
        	discordClient = event.from;
        	voiceConnection = discordClient->get_voice(guildId);
 
            /* Attempt to connect to a voice channel, returns false if we fail to connect. */
            if (!g->connect_member_voice(event.command.get_issuing_user().id)) {
                event.reply("You don't seem to be in a voice channel!");
                return;
            }
            
            /* Tell the user we joined their channel. */
            event.reply("Joined your channel!");
        } 
    });
 
    bot->on_ready([this](const dpp::ready_t & event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            /* Create a new command. */
            dpp::slashcommand joincommand("join", "Joins your voice channel.", bot->me.id);
 
            bot->global_bulk_command_create({ joincommand });
        }
    });
 
    /* Start bot */
    bot->start(dpp::st_return);
	
	Layer::OnAttach();
	
}

void BotLayer::OnDetach()
{
	bot->shutdown();
	Layer::OnDetach();
}

void BotLayer::BrowseFileOrFolder(std::string Path)
{
	if(Path.ends_with(".mp3"))
	{
		LoadAndPlaySong(Path);
	}
	else
	{
		PathToSongFolder = Path;
		RefreshFolderView();
	}
}

void BotLayer::GoUpInDirectoryView()
{
	size_t Index = PathToSongFolder.find_last_of('/');
	PathToSongFolder = PathToSongFolder.substr(0,Index);
	RefreshFolderView();
}

void BotLayer::PlaySong(std::vector<uint8_t>& songData)
{
	/* Stream the already decoded MP3 file. This passes the PCM data to the library to be encoded to OPUS */
	if(!voiceConnection)
	{
		voiceConnection = discordClient->get_voice(guildId);
	}
	voiceConnection->voiceclient->set_send_audio_type(dpp::discord_voice_client::satype_overlap_audio);
	voiceConnection->voiceclient->send_audio_raw((uint16_t*)songData.data(), songData.size());
}

void BotLayer::StopSong()
{
	voiceConnection->voiceclient->stop_audio();
}

void BotLayer::PauseOrUnpauseSong()
{
	voiceConnection->voiceclient->pause_audio(!voiceConnection->voiceclient->is_paused() ? true : false);
}

std::string BotLayer::GetSongDuration(const time_t& fullSeconds)
{
	uint8_t hours = (uint8_t)(fullSeconds/ 3600);
	uint8_t mins = (uint8_t)(fullSeconds % 3600 / 60);
	uint8_t secs = (uint8_t)(fullSeconds % 60);

	if (hours == 0) {
		char print_buffer[64];
		snprintf(print_buffer, 64, "%02d:%02d", mins, secs);
		return print_buffer;
	} else {
		char print_buffer[64];
		snprintf(print_buffer, 64, "%02d:%02d:%02d", hours, mins, secs);
		return print_buffer;
	}
}

std::string BotLayer::GetCurrentSongTime()
{
	float secondsRemaining = voiceConnection->voiceclient->get_secs_remaining();

	time_t CurrentTimeSeconds = SongLengthInSeconds - secondsRemaining;
	
	uint8_t hours = (uint8_t)(CurrentTimeSeconds/ 3600);
	uint8_t mins = (uint8_t)(CurrentTimeSeconds % 3600 / 60);
	uint8_t secs = (uint8_t)(CurrentTimeSeconds % 60);

	if (hours == 0) {
		char print_buffer[64];
		snprintf(print_buffer, 64, "%02d:%02d", mins, secs);
		return print_buffer;
	} else {
		char print_buffer[64];
		snprintf(print_buffer, 64, "%02d:%02d:%02d", hours, mins, secs);
		return print_buffer;
	}
}


Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Walnut Example";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<BotLayer>();
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	
	return app;
}
