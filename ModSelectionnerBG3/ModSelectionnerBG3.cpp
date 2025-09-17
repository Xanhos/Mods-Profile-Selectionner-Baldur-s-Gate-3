#include <iostream>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <thread>

#include "nfd.h"
#include "Tools.h"
#include "JSON/json.hpp"

using namespace nlohmann;

constexpr const char* SettingFileName = "Profile.ini";
constexpr const char* ProfilesHolderName = "Profiles";
constexpr const char* SettingsHolderName = "Settings";
constexpr const char* ModsListSettingsPath = "PlayerProfiles\\Public\\modsettings.lsx";
constexpr const char* ModListFilename = "modsettings.lsx";
constexpr const std::array<const wchar_t*, 4> BG3BinPossiblesNames = { L"bg3_dx11.exe" ,L"bg3.exe", L"Baldurs Gate 3.exe",	L"BaldursGate3.exe" };
constexpr int Indent = 4;

struct Settings
{
	std::string exec_mods_folder_path;
	std::string mods_storage_path;
};

struct Profile
{
	std::string name;
	std::string access_path;

	static Profile InvalidProfile;

	bool operator==(const Profile& profile) const
	{
		return this->access_path == profile.access_path && this->name == profile.name;
	}
};
Profile Profile::InvalidProfile = { "Invalid", "Invalid" };

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Profile, name, access_path)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Settings, exec_mods_folder_path, mods_storage_path)

namespace
{
	using Data = std::pair<Settings, std::vector<Profile>>;
	Data GlobalData = {};
	bool LeaveProgram;


	namespace Utils
	{

		Settings CreateSettings()
		{
			std::cout << "Please select your Baldur's Gate 3 mods folder \n";
			std::ostringstream oss;
			oss << GetCustomPath(FOLDERID_LocalAppData) << "\\Larian Studios\\Baldur's Gate 3\\Mods";
			std::string mods_folder = SelectFolder(oss.str().c_str());


			std::cout << "Please select your profiles storage folder\n";
			oss = {};
			std::string mods_storage = SelectFolder("C:\\");

			return Settings{ .exec_mods_folder_path = mods_folder,  .mods_storage_path = mods_storage };
		}

		void CreateDefaultProfile()
		{
			json parser;

			parser[SettingsHolderName] = CreateSettings();

			parser[ProfilesHolderName] = json::array();

			std::ofstream file(SettingFileName);
			file << parser.dump(Indent);
			file.close();

			std::ostringstream oss;
			oss << "Default " << SettingFileName << " Created with success !\n";
			std::cout << oss.str();
		}

		void CheckAndLoadProfile()
		{
			if (!fs::exists(SettingFileName))
			{
				std::ostringstream oss;
				oss << "Cannot find " << SettingFileName << ", creating default file\n";
				std::cout << oss.str();
				CreateDefaultProfile();
				std::cout << "\n\n";
			}

			json parser;
			Settings settings;
			std::vector<Profile> profiles;
			std::ifstream file(SettingFileName);
			file >> parser;
			try
			{
				profiles = parser.at(ProfilesHolderName).get<std::vector<Profile>>();
				settings = parser.at(SettingsHolderName).get<Settings>();
			}
			catch (const json::exception& e)
			{
				std::cerr << "Error: " << e.what() << "\n";
			}

			file.close();
			GlobalData = { settings,profiles };
		}

		void DisplayProfiles()
		{
			std::ostringstream oss;
			oss << GlobalData.second.size() << " profile found :\n"
				<< "\t0 - Go back to menu\n";
			std::cout << oss.str();

			int index = 1;
			for (auto profile : GlobalData.second)
			{
				oss = {};
				oss << "\t" << index++ << " - " << profile.name << "\n";
				std::cout << oss.str();
			}
		}


		Profile ChooseProfile()
		{
			DisplayProfiles();
			int choice = GetSecureNumericInput(0, static_cast<int>(GlobalData.second.size()), "Choose a profile by his number : ") - 1;
			if (choice == -1)
			{
				return Profile::InvalidProfile;
			}
			return GlobalData.second[choice];
		}


		void AddNewProfile(const Profile& new_profile)
		{
			std::ifstream ifile(SettingFileName);
			json parser;
			ifile >> parser;
			ifile.close();

			parser[ProfilesHolderName].push_back(new_profile);


			std::ofstream ofile(SettingFileName);
			ofile << parser.dump(Indent);
			ofile.close();

			CheckAndLoadProfile();
		}

		void RemoveProfile(const Profile& profile_to_delete)
		{
			std::ifstream ifile(SettingFileName);
			json parser;
			ifile >> parser;
			ifile.close();


			int index = 0;
			for (int i = 0; static_cast<size_t>(i) < parser[ProfilesHolderName].size(); i++)
			{
				if (parser[ProfilesHolderName][i].get<Profile>().name == profile_to_delete.name)
				{
					index = i;
					break;
				}
			}

			parser[ProfilesHolderName].erase(index);


			std::ofstream ofile(SettingFileName);
			ofile << parser.dump(Indent);
			ofile.close();

			CheckAndLoadProfile();
		}


		void CreateProfileDirectory(const Profile& profile)
		{
			if (!fs::exists(profile.access_path))
			{
				fs::create_directories(profile.access_path);
			}
			if (!fs::exists(profile.access_path + "\\Mods"))
			{
				fs::create_directories(profile.access_path + "\\Mods");
			}
		}

		void CopyCurrentMods(const Profile& profile)
		{
			if (profile == Profile::InvalidProfile)
			{
				return;
			}

			fs::remove_all(profile.access_path);
			CreateProfileDirectory(profile);

			fs::copy(GlobalData.first.exec_mods_folder_path, profile.access_path + "\\Mods", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
			fs::copy(GlobalData.first.exec_mods_folder_path + "\\..\\" + ModsListSettingsPath, profile.access_path, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
		}

		// Function to list all running processes (for debugging)
		void ListAllProcesses()
		{
			std::wcout << L"Listing all running processes:\n";
			PROCESSENTRY32W pe32;
			pe32.dwSize = sizeof(PROCESSENTRY32W);

			HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (hProcessSnap == INVALID_HANDLE_VALUE)
				return;

			if (Process32FirstW(hProcessSnap, &pe32))
			{
				do
				{
					std::wstring processName = pe32.szExeFile;
					if (processName.find(L"bg3") != std::wstring::npos ||
						processName.find(L"BG3") != std::wstring::npos ||
						processName.find(L"Baldur") != std::wstring::npos)
					{
						std::wcout << L"Found BG3-related process: " << processName << L" (PID: " << pe32.th32ProcessID << L")\n";
					}
				} while (Process32NextW(hProcessSnap, &pe32));
			}

			CloseHandle(hProcessSnap);
		}

		DWORD FindProcessByName(const std::wstring& processName)
		{
			PROCESSENTRY32W pe32;
			pe32.dwSize = sizeof(PROCESSENTRY32W);

			HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (hProcessSnap == INVALID_HANDLE_VALUE)
				return 0;

			DWORD processId = 0;
			if (Process32FirstW(hProcessSnap, &pe32))
			{
				do
				{
					if (processName == pe32.szExeFile)
					{
						processId = pe32.th32ProcessID;
						break;
					}
				} while (Process32NextW(hProcessSnap, &pe32));
			}

			CloseHandle(hProcessSnap);
			return processId;
		}

		bool WaitForProcessToClose(const std::wstring& processName, int timeoutSeconds = 0)
		{
			std::wcout << L"Waiting for " << processName << L" to start...\n";

			// Wait for the process to start (with 90 seconds timeout)
			DWORD processId = 0;
			int startupWaitTime = 90;
			while (processId == 0 && startupWaitTime > 0)
			{
				processId = FindProcessByName(processName);
				if (processId == 0)
				{
					std::this_thread::sleep_for(std::chrono::seconds(1));
					startupWaitTime--;
				}

				if (startupWaitTime % 10 == 0)
				{
					ListAllProcesses();
				}
			}

			if (processId == 0)
			{
				std::wcout << L"Process " << processName << L" was not found.\n";
				return false;
			}

			std::wcout << L"Process " << processName << L" detected (PID: " << processId << L"). Waiting for closure...\n";

			// Open the process to monitor its closure
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, processId);
			if (hProcess == NULL)
			{
				std::wcout << L"Unable to open process " << processName << L"\n";
				return false;
			}

			// Wait for the process to terminate
			DWORD waitResult;
			if (timeoutSeconds > 0)
			{
				waitResult = WaitForSingleObject(hProcess, timeoutSeconds * 1000);
			}
			else
			{
				waitResult = WaitForSingleObject(hProcess, INFINITE);
			}

			CloseHandle(hProcess);

			if (waitResult == WAIT_OBJECT_0)
			{
				std::wcout << L"Process " << processName << L" has terminated.\n";
				return true;
			}
			else if (waitResult == WAIT_TIMEOUT)
			{
				std::wcout << L"Timeout reached while waiting for " << processName << L" to close\n";
				return false;
			}
			else
			{
				std::wcout << L"Error while waiting for process closure.\n";
				return false;
			}
		}

		std::vector<DWORD> FindProcessesContaining(const std::wstring& substring)
		{
			std::vector<DWORD> processIds;
			PROCESSENTRY32W pe32;
			pe32.dwSize = sizeof(PROCESSENTRY32W);

			HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (hProcessSnap == INVALID_HANDLE_VALUE)
				return processIds;

			if (Process32FirstW(hProcessSnap, &pe32))
			{
				do
				{
					std::wstring processName = pe32.szExeFile;
					if (processName.find(substring) != std::wstring::npos)
					{
						processIds.push_back(pe32.th32ProcessID);
					}
				} while (Process32NextW(hProcessSnap, &pe32));
			}

			CloseHandle(hProcessSnap);
			return processIds;
		}


	}


	namespace Commands
	{
		void Leave();

		void SelectProfileAndLaunch()
		{
			Profile profile = Utils::ChooseProfile();
			if (profile == Profile::InvalidProfile)
			{
				return;
			}

			std::cout << "Loading " << profile.name << " profile...\n";

			fs::remove_all(GlobalData.first.exec_mods_folder_path);

			fs::copy(profile.access_path + "\\Mods", GlobalData.first.exec_mods_folder_path, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
			fs::copy(profile.access_path + "\\" + ModListFilename, GlobalData.first.exec_mods_folder_path + "\\..\\" + ModsListSettingsPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing);

			system("start steam://rungameid/1086940");

			std::cout << profile.name << " profile is now loaded ! Enjoy your game !\n";

			bool gameEnded = false;

			for (const auto& processName : BG3BinPossiblesNames)
			{
				if (Utils::WaitForProcessToClose(processName))
				{
					gameEnded = true;
					break;
				}
			}


			if (gameEnded)
			{
				std::cout << "Baldur's Gate 3 has closed. Saving profile...\n";
				Utils::CopyCurrentMods(profile);
			}
			else
			{
				std::cout << "Unable to detect BG3 process or timeout reached.\n";
				std::cout << "Would you like to save the profile manually ?";
				std::string result = getSecureStringInput(0, 1, true, "(y / n) :");
				if (result == "y" || result == "Y")
				{
					Utils::CopyCurrentMods(profile);
				}
			}

			Leave();
		}

		Profile CreateNewProfile()
		{
			std::string profile_name = getSecureStringInput(1, 25, false, "Enter a profile name (0 to go back to menu) : ");
			if (profile_name == "0")
			{
				return Profile::InvalidProfile;
			}

			std::ostringstream oss;
			oss << GlobalData.first.mods_storage_path << "\\" << profile_name;
			if (fs::exists(oss.str()))
			{
				std::cout << "This profile already exist !\n";
				return CreateNewProfile();
			}

			Profile new_profile{ .name = profile_name, .access_path = oss.str() };

			Utils::CreateProfileDirectory(new_profile);

			std::cout << "New profile " << profile_name << " created with success !\n";

			Utils::AddNewProfile(new_profile);

			return new_profile;
		}

		void CreateNewProfileFromCurrentMods()
		{
			Profile new_profile = CreateNewProfile();
			if (new_profile == Profile::InvalidProfile)
			{
				return;
			}

			if (!fs::exists(GlobalData.first.exec_mods_folder_path))
			{
				std::cout << "Baldur's Gate 3 mods folder doesn't exist ! Setup your settings first.\n";
				return;
			}
			if (!fs::exists(new_profile.access_path) || !fs::exists(new_profile.access_path + "\\Mods"))
			{
				std::cout << "this profile doesn't have a folder yet ! Creating a new one.\n";
				Utils::CreateProfileDirectory(new_profile);
			}

			Utils::CopyCurrentMods(new_profile);

			std::cout << "Mods copied from the current mods folder with success !\n";
		}




		void UpdateExistingProfileFromCurrentMods()
		{
			Profile profile = Utils::ChooseProfile();

			Utils::CopyCurrentMods(profile);
			std::cout << "Mods copied from the current mods folder with success !\n";
		}


		void DeleteProfile()
		{
			Profile profile = Utils::ChooseProfile();
			if (profile == Profile::InvalidProfile)
			{
				return;
			}

			std::cout << "Starting delete of " + profile.name << "\n";

			if (fs::exists(profile.access_path))
			{
				fs::remove_all(profile.access_path);
			}
			Utils::RemoveProfile(profile);

			std::cout << "Profile " << profile.name << " deleted with success :\n";
		}

		void SetupSettings()
		{
			std::ifstream ifile(SettingFileName);
			json parser;
			ifile >> parser;
			ifile.close();

			parser[SettingsHolderName] = Utils::CreateSettings();


			std::ofstream ofile(SettingFileName);
			ofile << parser.dump(Indent);
			ofile.close();

			Utils::CheckAndLoadProfile();
		}

		void Leave()
		{
			LeaveProgram = true;
		}
	}


	void MainLoop()
	{
		Utils::CheckAndLoadProfile();
		int choice = -1;
		while (!LeaveProgram)
		{
			std::cout << "1 - Select a Profile and launch the game\n"
				<< "2 - Create a new empty Profile\n"
				<< "3 - Create a new Profile from current mods folder\n"
				<< "4 - Update Profile from the current mods folder\n"
				<< "5 - Delete a Profile\n"
				<< "6 - Setup Settings\n"
				<< "0 - Leave\n";
			choice = GetSecureNumericInput(0, 5);
			std::system("CLS");

			switch (choice)
			{
			case 0:
				Commands::Leave();
				break;
			case 1:
				Commands::SelectProfileAndLaunch();
				break;
			case 2:
				Commands::CreateNewProfile();
				break;
			case 3:
				Commands::CreateNewProfileFromCurrentMods();
				break;
			case 4:
				Commands::UpdateExistingProfileFromCurrentMods();
				break;
			case 5:
				Commands::DeleteProfile();
				break;
			case 6:
				Commands::SetupSettings();
				break;
			default:
				break;
			}

			std::cout << "\n\n";
		}

	}




}



int main()
{
	MainLoop();
}