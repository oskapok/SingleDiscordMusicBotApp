#include "MainLayer.h"

#include <fstream>
#include <dpp/once.h>
#include "imgui.h"

constexpr uint_fast16_t SampleRate = 48000;
#pragma execution_character_set("utf-8")

void MainLayer::OnUIRender()
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

void MainLayer::RefreshFolderView()
{
	songPaths.clear();
	namespace fs = std::filesystem;
	for ( auto & entry : fs::directory_iterator(PathToSongFolder.c_str()))
	{
		//auto path = std::string(entry.path().string());
		//std::replace( path.begin(), path.end(), '\\', '/'); 
		//songPaths.emplace_back(path);
		//std::cout << entry.path() << std::endl;
	}
}

void MainLayer::CleanupDecoder()
{
	if(IsSongLoading)
	{
		IsSongLoading=false;
		delete[] buffer;
		mpg123_close(mpgHandle);
		mpg123_delete(mpgHandle);
		mpg123_exit();
	}
}

void MainLayer::OnUpdate(float ts)
{
	Layer::OnUpdate(ts);

	if(IsSongLoading)
	{
		std::vector<uint8_t> SongData;
		SongData.reserve(buffer_size * 8);
		for(int test = 0 ; test < 8; test ++)
		{
			if(mpg123_read(mpgHandle, buffer, buffer_size, &done) == MPG123_OK)
			{
				for (size_t i = 0; i < buffer_size; i++) {
					SongData.emplace_back(buffer[i]);
				}
			}
			else
			{
				CleanupDecoder();
				break;
			}
		}
		if(!SongData.empty())
		{
			PlaySong(SongData);
		}
	}
}

void MainLayer::OnAttach()
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

void MainLayer::OnDetach()
{
	bot->shutdown();
	Layer::OnDetach();
}

void MainLayer::BrowseFileOrFolder(std::string Path)
{
	if(Path.ends_with(".mp3"))
	{
		InitDecoder();
		PrepareSong(Path);
	}
	else
	{
		PathToSongFolder = Path;
		RefreshFolderView();
	}
}

void MainLayer::GoUpInDirectoryView()
{
	size_t Index = PathToSongFolder.find_last_of('/');
	PathToSongFolder = PathToSongFolder.substr(0,Index);
	RefreshFolderView();
}

void MainLayer::PlaySong(std::vector<uint8_t>& songData)
{
	/* Stream the already decoded MP3 file. This passes the PCM data to the library to be encoded to OPUS */
	if(!voiceConnection)
	{
		voiceConnection = discordClient->get_voice(guildId);
	}
	voiceConnection->voiceclient->set_send_audio_type(dpp::discord_voice_client::satype_overlap_audio);
	voiceConnection->voiceclient->send_audio_raw((uint16_t*)songData.data(), songData.size());
}

void MainLayer::StopSong()
{
	voiceConnection->voiceclient->stop_audio();
	CleanupDecoder();
}

void MainLayer::PauseOrUnpauseSong()
{
	voiceConnection->voiceclient->pause_audio(!voiceConnection->voiceclient->is_paused() ? true : false);
}

std::string MainLayer::GetSongDuration(const time_t& fullSeconds)
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

std::string MainLayer::GetCurrentSongTime()
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

void MainLayer::PrepareSong(const std::string& songPath)
{
	buffer = new unsigned char[buffer_size];
	
	mpg123_open(mpgHandle, songPath.c_str());
	mpg123_scan(mpgHandle);
	mpg123_getformat(mpgHandle, &rate, &channels, &encoding);

	SongLengthInSamples = mpg123_length(mpgHandle);
	SongLengthInSeconds = static_cast<double>(SongLengthInSamples) / SampleRate;

	CurrentSongLengthString = GetSongDuration(SongLengthInSeconds);
	CurrentlyPlayingSongName = songPath.substr(songPath.find_last_of("/\\") + 1);

	IsSongLoading = true;
}

void MainLayer::InitDecoder()
{
	mpg123_init();
	
	int err = 0;
	mpgHandle = mpg123_new(NULL, &err);
	mpg123_param(mpgHandle, MPG123_FORCE_RATE, SampleRate, SampleRate);
	
	buffer_size = mpg123_outblock(mpgHandle);
}

