// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "db/Registry.hxx"
#include "db/DatabasePlugin.hxx"
#include "db/Interface.hxx"
#include "db/Selection.hxx"
#include "db/DatabaseListener.hxx"
#include "db/LightDirectory.hxx"
#include "song/LightSong.hxx"
#include "db/PlaylistVector.hxx"
#include "ConfigGlue.hxx"
#include "tag/Config.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "event/Thread.hxx"
#include "util/ScopeExit.hxx"
#include "util/PrintException.hxx"

#include <fmt/core.h>

#include <stdexcept>

#include <stdlib.h>

class GlobalInit {
	EventThread io_thread;

public:
	GlobalInit() {
		io_thread.Start();
	}

	~GlobalInit() = default;

	EventLoop &GetEventLoop() {
		return io_thread.GetEventLoop();
	}
};

#ifdef ENABLE_UPNP
#include "input/InputStream.hxx"
size_t
InputStream::LockRead(void *, size_t)
{
	return 0;
}
#endif

class MyDatabaseListener final : public DatabaseListener {
public:
	void OnDatabaseModified() noexcept override {
		fmt::print("DatabaseModified\n");
	}

	void OnDatabaseSongRemoved(const char *uri) noexcept override {
		fmt::print("SongRemoved '{}'\n", uri);
	}
};

static void
DumpDirectory(const LightDirectory &directory)
{
	fmt::print("D {}\n", directory.GetPath());
}

static void
DumpSong(const LightSong &song)
{
	if (song.directory != nullptr)
		fmt::print("S {}/{}\n", song.directory, song.uri);
	else
		fmt::print("S {}\n", song.uri);
}

static void
DumpPlaylist(const PlaylistInfo &playlist, const LightDirectory &directory)
{
	fmt::print("P {}/{}\n", directory.GetPath(), playlist.name);
}

int
main(int argc, char **argv)
try {
	if (argc != 3) {
		fmt::print(stderr, "Usage: DumpDatabase CONFIG PLUGIN\n");
		return EXIT_FAILURE;
	}

	const FromNarrowPath config_path = argv[1];
	const char *const plugin_name = argv[2];

	const DatabasePlugin *plugin = GetDatabasePluginByName(plugin_name);
	if (plugin == nullptr) {
		fmt::print(stderr, "No such database plugin: {}\n", plugin_name);
		return EXIT_FAILURE;
	}

	/* initialize MPD */

	GlobalInit init;

	const auto config = AutoLoadConfigFile(config_path);

	TagLoadConfig(config);

	MyDatabaseListener database_listener;

	/* do it */

	const auto *path = config.GetParam(ConfigOption::DB_FILE);
	ConfigBlock block(path != nullptr ? path->line : -1);
	if (path != nullptr)
		block.AddBlockParam("path", path->value, path->line);

	auto db = plugin->create(init.GetEventLoop(),
				 init.GetEventLoop(),
				 database_listener, block);

	db->Open();

	AtScopeExit(&db) { db->Close(); };

	const DatabaseSelection selection("", true);

	db->Visit(selection, DumpDirectory, DumpSong, DumpPlaylist);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
