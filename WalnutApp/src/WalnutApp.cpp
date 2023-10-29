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


constexpr uint_fast16_t SampleRate = 48000;

class BotLayer : public Walnut::Layer
{
public:
	void LoadAndPlaySong(const std::string& songPath);

	virtual void OnUIRender() override
	{
		ImGui::Begin("Songs");
		for(auto songPath : songPaths)
		{
			if(ImGui::Button(songPath.c_str()))
			{
				LoadAndPlaySong(songPath);
			}
		}
		ImGui::End();


		ImGui::Begin("Media Controls");
		ImGui::Text(CurrentSongLengthString.c_str());
		
		if(ImGui::Button("StopSong"))
		{
			StopSong();
		}
		if(ImGui::Button("Pause/Unpause"))
		{
			PauseOrUnpauseSong();
		}
		ImGui::End();
	}

	void OnAttach() override;
	void OnDetach() override;

	void LoadSong(std::string songPath, std::vector<uint8_t>& SongData);
	void PlaySong(std::vector<uint8_t>& songData);
	void StopSong();
	void PauseOrUnpauseSong();

	std::string GetSongDuration(const double& fullSeconds);

private:
	std::vector<std::string> songPaths;
	dpp::cluster* bot;
	dpp::snowflake guildId;
	dpp::discord_client* discordClient = nullptr;

	long SongLengthInSamples = 0;
	double SongLengthInSeconds = 0;

	std::string CurrentSongLengthString;
	
};

void BotLayer::LoadAndPlaySong(const std::string& songPath)
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
}

void BotLayer::OnAttach()
{
	namespace fs = std::filesystem;
	std::string path = "C://BotSongs/";
	for (const auto & entry : fs::directory_iterator(path))
	{
		songPaths.emplace_back(entry.path().string());
		std::cout << entry.path() << std::endl;
	}

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

void BotLayer::PlaySong(std::vector<uint8_t>& songData)
{
	dpp::voiceconn* v = discordClient->get_voice(guildId);
	
	/* Stream the already decoded MP3 file. This passes the PCM data to the library to be encoded to OPUS */
	v->voiceclient->set_send_audio_type(dpp::discord_voice_client::satype_overlap_audio);
	v->voiceclient->send_audio_raw((uint16_t*)songData.data(), songData.size());
}

void BotLayer::StopSong()
{
	dpp::voiceconn* v = discordClient->get_voice(guildId);
	v->voiceclient->stop_audio();
}

void BotLayer::PauseOrUnpauseSong()
{
	dpp::voiceconn* v = discordClient->get_voice(guildId);
	v->voiceclient->pause_audio(!v->voiceclient->is_paused() ? true : false);
}

std::string BotLayer::GetSongDuration(const double& fullSeconds)
{
	int hours = fullSeconds/3600;
	int minutes = fullSeconds/60;
	int seconds = fullSeconds - (hours * 3600) - (minutes*60);


	std::string hoursString;
	std::string minutesString;
	std::string secondsString;
	if(hours < 10)
	{
		hoursString = "0" + std::to_string(hours);
	}
	if(minutes <10)
	{
		minutesString = "0" + std::to_string(minutes);
	}
	if(seconds <10)
	{
		secondsString = "0" + std::to_string(seconds);
	}
	
	return  hoursString+ ":"+ minutesString +":"+ secondsString;
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
