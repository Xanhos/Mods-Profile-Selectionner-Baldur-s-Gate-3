#pragma once
#include <algorithm>
#include <iostream>
#include <limits>
#include <concepts>
#include <functional>
#include <sstream>
#include <filesystem>



namespace fs = std::filesystem;

template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template<Numeric T>
T GetSecureNumericInput(T min, T max, const std::string& prompt = "Enter a number: ") {
	static_assert(std::is_arithmetic_v<T>, "Type must be arithmetic");

	T value;
	std::string input;

	while (true) {
		std::cout << prompt << "[" << min << " - " << max << "]: ";

		if (!std::getline(std::cin, input)) {
			std::cout << "\nReading error. Please try again.\n";
			std::cin.clear();
			continue;
		}

		if (input.empty()) {
			std::cout << "Empty input. Please enter a valid number.\n";
			continue;
		}

		std::stringstream ss(input);

		if (!(ss >> value)) {
			std::cout << "Invalid format. Please enter a valid number.\n";
			continue;
		}

		std::string remaining;
		if (ss >> remaining) {
			std::cout << "Extra characters detected. Please enter only a number.\n";
			continue;
		}

		if (value < min || value > max) {
			std::cout << "Value out of bounds. Number must be between "
				<< min << " and " << max << ".\n";
			continue;
		}

		return value;
	}
}

class StringValidator {
public:
	static bool notEmpty(const std::string& str) {
		return !str.empty();
	}

	static bool noWhitespaceOnly(const std::string& str) {
		return !str.empty() && !std::all_of(str.begin(), str.end(), ::isspace);
	}

	static bool alphabeticOnly(const std::string& str) {
		return !str.empty() && std::all_of(str.begin(), str.end(),
			[](char c) { return std::isalpha(c) || std::isspace(c); });
	}

	static bool alphanumericOnly(const std::string& str) {
		return !str.empty() && std::all_of(str.begin(), str.end(),
			[](char c) { return std::isalnum(c) || std::isspace(c); });
	}

	static bool noSpecialChars(const std::string& str) {
		return !str.empty() && std::all_of(str.begin(), str.end(),
			[](char c) { return std::isalnum(c) || std::isspace(c) || c == '-' || c == '_'; });
	}

	static std::function<bool(const std::string&)> lengthBetween(size_t min, size_t max) {
		return [min, max](const std::string& str) {
			return str.length() >= min && str.length() <= max;
			};
	}

	static std::function<bool(const std::string&)> minLength(size_t min) {
		return [min](const std::string& str) {
			return str.length() >= min;
			};
	}

	static std::function<bool(const std::string&)> maxLength(size_t max) {
		return [max](const std::string& str) {
			return str.length() <= max;
			};
	}
};


std::string getSecureStringInput(
	const std::vector<std::function<bool(const std::string&)>>& validators,
	const std::vector<std::string>& errorMessages,
	const std::string& prompt = "Enter a string: "
) {
	if (validators.size() != errorMessages.size()) {
		throw std::invalid_argument("Number of validators must match number of error messages");
	}

	std::string input;

	while (true) {
		std::cout << prompt;

		// Read the entire line
		if (!std::getline(std::cin, input)) {
			// Handle EOF or stream error
			std::cout << "Reading error. Please try again.\n";
			std::cin.clear();
			continue;
		}

		// Validate against all provided validators
		bool isValid = true;
		for (size_t i = 0; i < validators.size(); ++i) {
			if (!validators[i](input)) {
				std::cout << errorMessages[i] << "\n";
				isValid = false;
				break; // Stop at first validation failure
			}
		}

		if (isValid) {
			return input;
		}
	}
}

// Simplified overload for common cases
std::string getSecureStringInput(
	size_t minLength,
	size_t maxLength,
	bool allowOnlyAlphabetic = false,
	const std::string& prompt = "Enter a string: "
) {
	std::vector<std::function<bool(const std::string&)>> validators;
	std::vector<std::string> errorMessages;

	// Add length validation
	validators.push_back(StringValidator::lengthBetween(minLength, maxLength));
	errorMessages.push_back("String length must be between " + std::to_string(minLength) +
		" and " + std::to_string(maxLength) + " characters.");

	// Add non-empty validation
	validators.push_back(StringValidator::noWhitespaceOnly);
	errorMessages.push_back("Input cannot be empty or contain only whitespace.");

	// Add alphabetic validation if requested
	if (allowOnlyAlphabetic) {
		validators.push_back(StringValidator::alphabeticOnly);
		errorMessages.push_back("Input must contain only alphabetic characters and spaces.");
	}

	return getSecureStringInput(validators, errorMessages, prompt);
}



std::string SelectFolder(const char* defaultPath)
{
	if (!fs::exists(fs::path(defaultPath)))
	{
		defaultPath = "C:\\";
	}

	nfdchar_t* outPath = nullptr;
	nfdresult_t result = NFD_PickFolder(defaultPath, &outPath);

	if (result == NFD_OKAY) {

		std::string string{ outPath };
		free(outPath);
		return string;
	}
	return std::string();
}

std::string SelectFile(const char* filter, const char* defaultPath)
{
	if (!fs::exists(fs::path(defaultPath)))
	{
		defaultPath = "C:\\";
	}

	nfdchar_t* outPath = nullptr;
	nfdresult_t result = NFD_OpenDialog(filter, defaultPath, &outPath);

	if (result == NFD_OKAY) {
		std::string string{ outPath };
		free(outPath);
		return string;
	}
	return std::string();
}

std::string GetCustomPath(GUID rfid) {
	PWSTR path;
	if (SUCCEEDED(SHGetKnownFolderPath(rfid, 0, NULL, &path))) {
		int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
		std::string result(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], size, NULL, NULL);
		CoTaskMemFree(path);
		result.pop_back();
		return result;
	}
	return "";
}
