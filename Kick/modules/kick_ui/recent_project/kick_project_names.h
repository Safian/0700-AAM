// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) project name rules and the "new project from a template" decision, with the file system injected so the
// collision / missing file / half-made folder cases are host-tested: Kick/tests/platform/ProjectNamesTests.cpp.
#pragma once

#include <string>

namespace kick_project
{
    constexpr size_t kMaxNameLength = 64;

    enum class NameProblem { Ok, Empty, TooLong, BadCharacter, DotsOnly };

    // Spaces at both ends are not part of the name.
    inline std::string trimName( const std::string& s )
    {
        const char* ws = " \t\r\n";
        const auto  b  = s.find_first_not_of( ws );
        if( b == std::string::npos )
            return {};
        return s.substr( b, s.find_last_not_of( ws ) - b + 1 );
    }

    // The name becomes a folder (and the project file inside it) on the internal flash, an SD card or a FAT USB stick: no path
    // separators, none of the characters FAT forbids, no control characters, not "." / "..", no trailing dot.
    inline NameProblem checkName( const std::string& trimmedName )
    {
        if( trimmedName.empty() )
            return NameProblem::Empty;
        if( trimmedName.size() > kMaxNameLength )
            return NameProblem::TooLong;
        for( unsigned char c : trimmedName )
            if( c < 0x20 || c == 0x7f || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' )
                return NameProblem::BadCharacter;
        if( trimmedName.find_first_not_of( '.' ) == std::string::npos )
            return NameProblem::DotsOnly;
        if( trimmedName.back() == '.' )
            return NameProblem::BadCharacter;
        return NameProblem::Ok;
    }

    inline const char* describe( NameProblem p )
    {
        switch( p )
        {
            case NameProblem::Ok: return "";
            case NameProblem::Empty: return "Please type a project name";
            case NameProblem::TooLong: return "The project name is too long (64 characters at most)";
            case NameProblem::BadCharacter: return "The project name must not contain / \\ : * ? \" < > | and must not end with a dot";
            case NameProblem::DotsOnly: return "The project name must have letters or digits";
        }
        return "";
    }

    enum class CreateResult { Ok, BadName, AlreadyExists, NoTemplateFile, CopyFailed };

    inline const char* describe( CreateResult r )
    {
        switch( r )
        {
            case CreateResult::Ok: return "";
            case CreateResult::BadName: return "The project name is not valid";
            case CreateResult::AlreadyExists: return "A project with the same name already exists";
            case CreateResult::NoTemplateFile: return "The template has no project file";
            case CreateResult::CopyFailed: return "The template could not be copied (is the drive full or read-only?)";
        }
        return "";
    }

    // Fs: bool exists(path), bool hasTemplateProject(), bool makeFolder(path), bool copyTemplateInto(folder), void removeFolder(path).
    // The project folder is `root/name`. Nothing that exists on disk is touched; a folder this call made is removed again when
    // the copy fails, so no empty or half-filled project folder is left behind.
    template<class Fs>
    CreateResult createFromTemplate( const std::string& rawName, const std::string& root, Fs& fs, std::string* folderOut = nullptr )
    {
        const auto name = trimName( rawName );
        if( checkName( name ) != NameProblem::Ok )
            return CreateResult::BadName;
        const std::string folder = root + "/" + name;
        if( fs.exists( folder ) )
            return CreateResult::AlreadyExists;
        if( !fs.hasTemplateProject() )
            return CreateResult::NoTemplateFile;
        if( !fs.makeFolder( folder ) )
            return CreateResult::CopyFailed;
        if( !fs.copyTemplateInto( folder ) )
        {
            fs.removeFolder( folder );
            return CreateResult::CopyFailed;
        }
        if( folderOut )
            *folderOut = folder;
        return CreateResult::Ok;
    }

    // Name of the project made at start-up ("New Project" on launch): "<prefix> <stamp>", stamp like "2026-11-04 15.30" (no ':' so
    // it is legal on FAT). An empty prefix gives "Project". Cut to the length limit and made free with " 2", " 3" ...
    template<class Exists>
    std::string launchProjectName( const std::string& prefix, const std::string& stamp, Exists exists )
    {
        std::string base = trimName( prefix.empty() ? "Project " + stamp : prefix + " " + stamp );
        std::string clean;
        for( unsigned char c : base )
            clean += ( c < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ) ? '-' : static_cast<char>( c );
        if( clean.size() > kMaxNameLength - 4 )
            clean.resize( kMaxNameLength - 4 );
        clean = trimName( clean );
        while( !clean.empty() && clean.back() == '.' )
            clean.pop_back();
        if( clean.empty() )
            clean = "Project";
        if( !exists( clean ) )
            return clean;
        for( int n = 2; n < 1000; ++n )
            if( !exists( clean + " " + std::to_string( n ) ) )
                return clean + " " + std::to_string( n );
        return clean;
    }
}
