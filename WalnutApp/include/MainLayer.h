#pragma once
#include <mpg123.h>
#include <string>
#include <vector>
#include <dpp/cluster.h>

#include "Walnut/Layer.h"


class MainLayer : public Walnut::Layer
{
public:
    virtual void OnUIRender() override;

    void OnUpdate(float ts) override;

    void OnAttach() override;
    void OnDetach() override;
    void BrowseFileOrFolder(std::string Path);
    void GoUpInDirectoryView();
	
    void RefreshFolderView();
    void CleanupDecoder();
    void PlaySong(std::vector<uint8_t>& songData);
    void StopSong();
    void PauseOrUnpauseSong();

    std::string GetSongDuration(const time_t& fullSeconds);
    std::string GetCurrentSongTime();

    void PrepareSong(const std::string& songPath);
    void InitDecoder();

private:
    std::vector<std::string> songPaths;
    dpp::cluster* bot = nullptr;
    dpp::snowflake guildId;
    dpp::voiceconn* voiceConnection = nullptr;
    dpp::discord_client* discordClient = nullptr;
	
    bool IsSongLoading = false;
    long SongLengthInSamples = 0;
    double SongLengthInSeconds = 0;

    std::string CurrentSongLengthString;
    std::string PathToSongFolder = "C://BotSongs/";
    std::string CurrentlyPlayingSongName;

    mpg123_handle* mpgHandle = nullptr;
    unsigned char* buffer = nullptr;
    size_t buffer_size = 0;
    int channels = 0, encoding = 0;
    long rate = 0;
    size_t done = 0;
};
