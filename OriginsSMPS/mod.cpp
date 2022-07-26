#include "pch.h"
#include <map>
#include "HiteModLoader.h"
#include "Helpers.h"
#include "SigScan.h"
#include "IniFile.hpp"
#include "SMPSPlay.h"

// Data pointer and array declarations.
#define DataPointer(type, name, address) \
	static type &name = *(type *)address
#define DataArray(type, name, address, len) \
	static DataArray_t<type, len> name(address)

template<typename T, size_t len>
struct DataArray_t final
{
	typedef T value_type;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef value_type* pointer;
	typedef const value_type* const_pointer;
	typedef pointer iterator;
	typedef const_pointer const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	DataArray_t() = default; // have to declare default constructor
	DataArray_t(const DataArray_t&) = delete; // object cannot be copied, prevents accidentally using DataArray in a function call
	DataArray_t(const DataArray_t&&) = delete; // object cannot be moved

	DataArray_t(intptr_t addr)
	{
		this->addr = addr;
	}

	// Gets the underlying data for the array.
	constexpr pointer data() const noexcept { return reinterpret_cast<pointer>(addr); }
	// Gets the underlying data for the array.
	constexpr const_pointer cdata() const noexcept { return reinterpret_cast<const_pointer>(addr); }

	// Checks if the array is empty (no elements).
	constexpr bool empty() const noexcept { return len == 0; }

	// Gets the size of the array, in elements.
	constexpr size_type size() const noexcept { return len; }

	// Gets the maximum size of the array, in elements.
	constexpr size_type max_size() const noexcept { return len; }

	constexpr pointer operator&() const noexcept { return data(); }

	constexpr operator pointer() const noexcept { return data(); }

	// Gets an item from the array, with bounds checking.
	constexpr reference at(size_type i)
	{
		if (i < len)
			return data()[i];
		throw std::out_of_range("Data access out of range.");
	}

	// Gets an item from the array, with bounds checking.
	constexpr const_reference at(size_type i) const
	{
		if (i < len)
			return cdata()[i];
		throw std::out_of_range("Data access out of range.");
	}

	template<size_type I>
	// Gets an item from the array, with compile-time bounds checking.
	constexpr reference get() noexcept
	{
		static_assert(I < len, "index is within bounds");
		return data()[I];
	}

	template<size_type I>
	// Gets an item from the array, with compile-time bounds checking.
	constexpr const_reference get() const noexcept
	{
		static_assert(I < len, "index is within bounds");
		return cdata()[I];
	}

	// Gets the first item in the array.
	constexpr reference front() { return *data(); }
	// Gets the first item in the array.
	constexpr const_reference front() const { return *cdata(); }

	// Gets the last item in the array.
	constexpr reference back() { return data()[len - 1]; }
	// Gets the last item in the array.
	constexpr const_reference back() const { return cdata()[len - 1]; }

	// Gets an iterator to the beginning of the array.
	constexpr iterator begin() noexcept { return data(); }
	// Gets an iterator to the beginning of the array.
	constexpr const_iterator begin() const noexcept { return cdata(); }
	// Gets an iterator to the beginning of the array.
	constexpr const_iterator cbegin() const noexcept { return cdata(); }

	// Gets an iterator to the end of the array.
	constexpr iterator end() noexcept { return data() + len; }
	// Gets an iterator to the end of the array.
	constexpr const_iterator end() const noexcept { return cdata() + len; }
	// Gets an iterator to the end of the array.
	constexpr const_iterator cend() const noexcept { return cdata() + len; }

	// Gets a reverse iterator to the beginning of the array.
	constexpr reverse_iterator rbegin() noexcept { return data() + len; }
	// Gets a reverse iterator to the beginning of the array.
	constexpr const_reverse_iterator rbegin() const noexcept { return cdata() + len; }
	// Gets a reverse iterator to the beginning of the array.
	constexpr const_reverse_iterator crbegin() const noexcept { return cdata() + len; }

	// Gets a reverse iterator to the end of the array.
	constexpr reverse_iterator rend() noexcept { return data(); }
	// Gets a reverse iterator to the end of the array.
	constexpr const_reverse_iterator rend() const noexcept { return cdata(); }
	// Gets a reverse iterator to the end of the array.
	constexpr const_reverse_iterator crend() const noexcept { return cdata(); }

	private:
		intptr_t addr;
};

ModLoader* ModLoaderData;
const char** ModPaths;
int ModPathCount;
std::map<std::string, short> songNames;
bool issmps = false;
char currentmusic[MAX_PATH]{};
bool isfast = false;

HOOK(void, __fastcall, StopMusic, ReadDataPointer((size_t)SigS3KSetup() + 0x899, 3, 7), __int64 channel)
{
	if (issmps)
	{
		SMPS_StopSong();
		issmps = false;
		currentmusic[0] = 0;
	}
	else
		originalStopMusic(channel);
}

HOOK(__int64, __fastcall, PlayMusic, SigPlayMusic(), const char *name, __int64 channel, __int64 startpos, unsigned int looppos, int async)
{
	if (!name[0])
		return -1;

	bool speedup = false;
	const char* _name = name;
	char origname[MAX_PATH];
	const char* tmp = strstr(name, "_F.ogg");
	if (tmp)
	{
		strcpy(origname, name);
		strcpy(origname + (tmp - name), ".ogg");
		speedup = true;
		_name = origname;
	}
	else
	{
		tmp = strstr(name, "/F/");
		if (tmp)
		{
			strcpy(origname, name);
			strcpy(origname + (tmp - name), tmp + 2);
			speedup = true;
			_name = origname;
		}
	}

	if (!strcmp(currentmusic, _name))
	{
		if (speedup)
		{
			SMPS_SetSongTempo(1.25);
			isfast = true;
			return -1;
		}
		else if (isfast)
		{
			SMPS_SetSongTempo(1);
			isfast = false;
			return -1;
		}
	}

	implOfStopMusic(channel);
	
	for (int i = 0; i < ModPathCount; i++)
	{
		char fn2[MAX_PATH];
		snprintf(fn2, MAX_PATH, "%s\\SMPSMusic.ini", ModPaths[i]);
		if (FileExists(fn2))
		{
			IniFile ini(fn2);
			auto iter = songNames.find(ini.getString(ModLoaderData->GetDataPackName(), _name));
			if (iter != songNames.end())
			{
				issmps = true;
				strcpy(currentmusic, _name);
				isfast = speedup;
				SMPS_LoadAndPlaySong(iter->second);
				if (isfast)
					SMPS_SetSongTempo(1.25);
				return -1;
			}
		}
	}
	currentmusic[0] = 0;
	return originalPlayMusic(name, channel, startpos, looppos, async);
}

extern "C" __declspec(dllexport) void Init(ModInfo *modInfo)
{
	ModLoaderData = modInfo->ModLoader;

	if (ModLoaderData == (void*)1 /*|| ModLoaderData->MajorVersion < 1 || (ModLoaderData->MajorVersion == 1 && ModLoaderData->MinorVersion < 1)*/)
	{
		MessageBoxW(nullptr, L"The installed version of HiteModLoader is too old for OriginsSMPS to run,\nplease update the modloader to " ML_VERSION " or newer.", L"OriginsSMPS Error", MB_ICONERROR);
		return;
	}

	// Check signatures
	if (!SigValid)
	{
		MessageBoxW(nullptr, L"Signature Scan Failed!\n\nThis usually means there is a conflict or the mod is running on an incompatible game version.", L"Scan Error", MB_ICONERROR);
		return;
	}

	if (!SMPS_InitializeDriver())
	{
		MessageBoxW(nullptr, L"SMPS player failed to initialize!", L"SMPS Error", MB_ICONERROR);
		return;
	}

	SMPS_SetVolume(0.5);

	ModPathCount = ModLoaderData->GetIncludePaths(nullptr, 0);
	ModPaths = new const char* [ModPathCount];
	ModLoaderData->GetIncludePaths(ModPaths, ModPathCount);

	unsigned int songcnt;
	const char** songs = SMPS_GetSongNames(songcnt);
	for (short i = 0; i < songcnt; i++)
		songNames.insert({ songs[i], i });

	INSTALL_HOOK(PlayMusic);
	INSTALL_HOOK(StopMusic);
}