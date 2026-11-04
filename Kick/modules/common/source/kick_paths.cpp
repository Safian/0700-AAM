/*
 * Copyright (c) 2022 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

//common
#define MT_SOURCE_SRCDIR "."

juce::File getAppDirs( DirType T )
{
    const auto resource = juce::SystemStats::getEnvironmentVariable("KICK_RESOURCE_ROOT", {});
    const auto data = juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {});
    if (resource.isNotEmpty()) {
        const auto root = juce::File::isAbsolutePath(resource) ? juce::File(resource) : juce::File::getCurrentWorkingDirectory().getChildFile(resource);
        if (T == MTAssets) return root.getChildFile("assets");
        if (T == MTTemp) return root.getChildFile("templates");
        if (T == MTDemos) return root.getChildFile("demos");
    }
    if (data.isNotEmpty()) {
        const auto root = juce::File::isAbsolutePath(data) ? juce::File(data) : juce::File::getCurrentWorkingDirectory().getChildFile(data);
        if (T == MTPrefs) return root.getChildFile("prefs");
        if (T == MTProjects) return root.getChildFile("projects");
        if (T == MTLog) return root.getChildFile("logs");
        if (T == MTSamples) return root.getChildFile("sounds");
    }
    auto seprator = juce::File::getSeparatorString();
#if defined( JUCE_IOS )
    auto bundleDir = juce::File::getSpecialLocation( juce::File::currentApplicationFile );
    auto userDocs  = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory ).getChildFile( "Kick" );
    if( T == MTAssets )
        return bundleDir.getChildFile( "assets" );
    else if( T == MTDemos )
        return bundleDir.getChildFile( "demos" );
    else if( T == MTTemp )
        return bundleDir.getChildFile( "templates" );
    else if( T == MTProjects )
    {
        auto dir = userDocs.getChildFile( "projects" );
        dir.createDirectory();
        return dir;
    }
    else if( T == MTSamples )
    {
        auto dir = userDocs.getChildFile( "sounds" );
        dir.createDirectory();
        return dir;
    }
    else if( T == MTLog )
    {
        auto dir = userDocs.getChildFile( "logs" );
        dir.createDirectory();
        return dir;
    }
    else if( T == MTPrefs )
    {
        auto dir = userDocs.getChildFile( "prefs" );
        dir.createDirectory();
        return dir;
    }
#elif defined( _WIN32 ) || defined( _WIN64 )
    auto rootP = juce::File::getSpecialLocation( juce::File::currentApplicationFile ).getParentDirectory().getFullPathName() + seprator;
    if( T == MTAssets )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "assets" );
    else if( T == MTSamples )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "samples" );
    else if( T == MTDemos )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "demos" );
    else if( T == MTTemp )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "templates" );
    else if( T == MTProjects )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "projects" );
    else if( T == MTLog )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "logs" + seprator );
    else if( T == MTPrefs )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "prefs" + seprator );
#elif defined( __APPLE_CPP__ ) || defined( __APPLE_CC__ ) || defined( JUCE_MAC )
    auto appParent = juce::File::getSpecialLocation( juce::File::currentApplicationFile ).getParentDirectory();
    auto resolveMacPath = [&] (const juce::String& rel) -> juce::File
    {
        juce::File local (rel);
        if (local.exists())
            return local;
        auto relClean = rel.startsWith ("./") ? rel.substring (2) : rel;
        auto fromApp = appParent.getChildFile (relClean);
        if (fromApp.exists())
            return fromApp;
        auto fromKick = appParent.getParentDirectory().getChildFile ("Kick").getChildFile (relClean);
        if (fromKick.exists())
            return fromKick;
        return local;
    };
    if( T == MTAssets )
        return resolveMacPath( "./assets" );
    else if( T == MTSamples )
        return resolveMacPath( "./data/samplesKick/" );
    else if( T == MTDemos )
        return resolveMacPath( "./demos" );
    else if( T == MTTemp )
        return resolveMacPath( "./templates" );
    else if( T == MTProjects )
        return resolveMacPath( "./projects" );
    else if( T == MTLog )
        return resolveMacPath( "./logs/" );
    else if( T == MTPrefs )
        return resolveMacPath( "./prefs" );
#elif defined( LINUX ) || defined( __linux__ )
    auto resolveLinuxPath = [] (const juce::String& primary, const juce::String& fallback) -> juce::File
    {
        juce::File prim (primary);
        if (prim.exists())
            return prim;
        return juce::File (fallback);
    };

    auto makeDir = [] (const juce::File& f) -> juce::File
    {
        if (!f.exists())
            f.createDirectory();
        return f;
    };

    if( T == MTAssets )
        return resolveLinuxPath( "/data/assets", "/app/assets" );
    else if( T == MTSamples )
        return makeDir( resolveLinuxPath( "/data/sounds", "/app/sounds" ) );
    else if( T == MTDemos )
        return resolveLinuxPath( "/data/demos", "/app/demos" );
    else if( T == MTTemp )
        return resolveLinuxPath( "/data/templates", "/app/templates" );
    else if( T == MTProjects )
        return makeDir( resolveLinuxPath( "/data/projects", "/home/Kick/projects" ) );
    else if( T == MTLog )
        return makeDir( resolveLinuxPath( "/data/logs", "/home/Kick/logs" ) );
    else if( T == MTPrefs )
    {
        // /data is the writable partition of the device; the fallback is on the read-only root file system, where the
        // preferences, menu layout and insert presets could never be saved (MPC, 2026-11-04: no /data/prefs, brightness 0, empty settings).
        const juce::File data( "/data" );
        if( data.isDirectory() )
            return makeDir( data.getChildFile( "prefs" ) );
        return makeDir( resolveLinuxPath( "/data/prefs", "/home/Kick/prefs" ) );
    }
#endif

    return juce::File::getSpecialLocation( juce::File::tempDirectory );
}

juce::File getDefaultPath()
{
    juce::File rootP;
    auto       seprator = juce::File::getSeparatorString();

#if defined( JUCE_IOS )
    rootP = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory ).getChildFile( "Kick" );
    rootP.createDirectory();
#elif defined( _WIN32 ) || defined( _WIN64 )
    rootP = juce::File::getSpecialLocation( juce::File::currentApplicationFile ).getParentDirectory().getFullPathName() + seprator;

#elif defined( __APPLE_CPP__ ) || defined( __APPLE_CC__ ) || defined( JUCE_MAC )
    rootP = "./app/";

#elif defined( LINUX ) || defined( __linux__ )
    rootP = "./app/";
#endif
    return rootP;
}
