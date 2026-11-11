// SPDX-License-Identifier: AGPL-3.0-or-later
// Host unit tests for Google Drive helper functions (PKCE S256 RFC 7636 vector, Drive URLs/IDs, RFC3339 timestamps, creds).
#include "../../modules/kick_ui/modes/filebrowser/kick_googleDriveHelper.h"

#include <cstdio>
#include <string>

using namespace ui::fileSource::gdrive;

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

int main()
{
    // RFC 7636 Appendix B official PKCE S256 test vector
    {
        const std::string verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
        const std::string expected = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";
        CHECK( pkceS256Challenge( verifier ) == expected );
    }

    // Query percent-encoding round-trip
    {
        const std::string orig = "Drum Loops & FX/808's+100%";
        CHECK( decodeQueryComponent( encodeQueryComponent( orig ) ) == orig );
    }

    // Reversed client ID scheme
    CHECK( buildReversedClientScheme( "123456-abcDEF.apps.googleusercontent.com" ) == "com.googleusercontent.apps.123456-abcDEF" );
    CHECK( buildReversedClientScheme( "not-a-google-client-id" ).empty() );

    // Folder link / ID parsing
    {
        auto t = parseFolderTarget( "root" );
        CHECK( t.ok && t.folderId == "root" );

        t = parseFolderTarget( "gdrive://folder/1AbCdEfGhIjKlMnOpQr" );
        CHECK( t.ok && t.folderId == "1AbCdEfGhIjKlMnOpQr" );

        t = parseFolderTarget( "https://drive.google.com/drive/u/0/folders/1AbCdEfGhIjKlMnOpQr?resourcekey=0-XYZ_99&usp=sharing" );
        CHECK( t.ok && t.folderId == "1AbCdEfGhIjKlMnOpQr" && t.resourceKey == "0-XYZ_99" );

        t = parseFolderTarget( "https://drive.google.com/open?id=1OpenLinkFolderId_99" );
        CHECK( t.ok && t.folderId == "1OpenLinkFolderId_99" );

        t = parseFolderTarget( "bad" );
        CHECK( !t.ok );
    }

    // Auth URL and callback extraction
    {
        const auto authUrl = buildAuthUrl( "123-abc.apps.googleusercontent.com",
                                           "com.googleusercontent.apps.123-abc:/oauth2redirect",
                                           "CHAL123",
                                           "ST456",
                                           "producer@gmail.com" );
        CHECK( authUrl.find( "client_id=123-abc.apps.googleusercontent.com" ) != std::string::npos );
        CHECK( authUrl.find( "login_hint=producer%40gmail.com" ) != std::string::npos );
        CHECK( authUrl.find( "code_challenge_method=S256" ) != std::string::npos );

        const auto cb = parseOAuthCallbackUrl( "com.googleusercontent.apps.123-abc:/oauth2redirect?state=ST456&code=4%2F0AfJohX" );
        CHECK( cb.ok() && cb.state == "ST456" && cb.code == "4/0AfJohX" );

        const auto denied = parseOAuthCallbackUrl( "com.googleusercontent.apps.123-abc:/oauth2redirect?error=access_denied&error_description=User%20cancelled" );
        CHECK( !denied.ok() && denied.error == "access_denied" && denied.errorDescription == "User cancelled" );
    }

    // Files list & get URLs with escaped apostrophe
    {
        const auto u = buildFilesListUrl( "1RootFolderId", false, "Mike's Kit", "tok2", "KEY9" );
        CHECK( u.find( "https://www.googleapis.com/drive/v3/files?" ) == 0 );
        CHECK( u.find( "pageToken=tok2" ) != std::string::npos );
        CHECK( u.find( "key=KEY9" ) != std::string::npos );
        CHECK( escapeDriveQueryLiteral( "Bob's\\Drums" ) == "Bob\\'s\\\\Drums" );

        const auto su = buildFilesListUrl( "__shared__", true, {}, {}, {} );
        CHECK( su.find( "sharedWithMe" ) != std::string::npos );

        const auto gu = buildFileGetUrl( "file123", true, "KEY9" );
        CHECK( gu == "https://www.googleapis.com/drive/v3/files/file123?alt=media&supportsAllDrives=true&key=KEY9" );
    }

    // RFC 3339 date parsing
    CHECK( parseRfc3339Ms( "1970-01-01T00:00:00Z" ) == 0 );
    CHECK( parseRfc3339Ms( "2026-11-11T03:00:00.250Z" ) == 1794366000250ll );
    CHECK( parseRfc3339Ms( "2026-11-11T04:00:00+01:00" ) == 1794366000000ll );
    CHECK( parseRfc3339Ms( "not-a-date" ) == 0 );

    // Credential serialization / parsing
    {
        Credentials c;
        c.mode         = "oauth";
        c.clientId     = "123.apps.googleusercontent.com";
        c.accessToken  = "ya29.access";
        c.refreshToken = "1//refresh";
        c.expiresAtMs  = 1794369600000ll;
        c.email        = "szabi@gmail.com";
        const auto s   = serializeCredentials( c );
        const auto r   = parseCredentials( s );
        CHECK( r.mode == "oauth" && r.clientId == c.clientId && r.accessToken == c.accessToken && r.refreshToken == c.refreshToken && r.expiresAtMs == c.expiresAtMs && r.email == c.email );

        const auto rawRef = parseCredentials( "1//0abc\n" );
        CHECK( rawRef.mode == "oauth" && rawRef.refreshToken == "1//0abc" );
    }

    std::printf( g_fail == 0 ? "PASS GoogleDriveHelperTests\n" : "FAIL (%d)\n", g_fail );
    return g_fail == 0 ? 0 : 1;
}
