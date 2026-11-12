// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_phonePairingServer.h"

namespace ui
{
    PhonePairingServer::PhonePairingServer( int preferredPort )
        : juce::Thread( "KickPhonePairingServer" )
    {
        lanIp = discoverLanIp();
        for( int p = preferredPort; p < preferredPort + 10; ++p )
        {
            auto sock = std::make_unique<juce::StreamingSocket>();
            if( sock->createListener( p, juce::String() ) )
            {
                listenerSocket = std::move( sock );
                activePort     = p;
                running        = true;
                break;
            }
        }
        if( running.load() )
            startThread();
    }

    PhonePairingServer::~PhonePairingServer()
    {
        running = false;
        signalThreadShouldExit();
        if( listenerSocket != nullptr )
            listenerSocket->close();
        stopThread( 1500 );
        listenerSocket.reset();
    }

    juce::String PhonePairingServer::getUrl() const
    {
        return "http://" + lanIp + ":" + juce::String( activePort ) + "/";
    }

    juce::String PhonePairingServer::discoverLanIp()
    {
        const auto addrs = juce::IPAddress::getAllAddresses();
        juce::String candidate;
        for( const auto& a : addrs )
        {
            const auto s = a.toString();
            if( s == "127.0.0.1" || s.contains( ":" ) || s.isEmpty() )
                continue;
            // Prefer common private network subnets
            if( s.startsWith( "192.168." ) || s.startsWith( "10." ) || s.startsWith( "172." ) )
                return s;
            if( candidate.isEmpty() )
                candidate = s;
        }
        return candidate.isNotEmpty() ? candidate : juce::String( "127.0.0.1" );
    }

    void PhonePairingServer::run()
    {
        while( !threadShouldExit() && listenerSocket != nullptr )
        {
            auto* client = listenerSocket->waitForNextConnection();
            if( client == nullptr )
            {
                if( threadShouldExit() )
                    break;
                wait( 50 );
                continue;
            }
            handleClient( std::unique_ptr<juce::StreamingSocket>( client ) );
        }
    }

    void PhonePairingServer::handleClient( std::unique_ptr<juce::StreamingSocket> client )
    {
        if( client == nullptr )
            return;

        std::vector<char> buf( 65536, 0 );
        int bytesRead = client->read( buf.data(), static_cast<int>( buf.size() - 1 ), false );
        if( bytesRead <= 0 )
            return;
        buf[static_cast<size_t>( bytesRead )] = 0;

        juce::String reqStr( buf.data() );
        const auto firstLineEnd = reqStr.indexOf( "\r\n" );
        if( firstLineEnd < 0 )
            return;

        const auto firstLine = reqStr.substring( 0, firstLineEnd );
        auto tokens = juce::StringArray::fromTokens( firstLine, " ", "" );
        if( tokens.size() < 2 )
            return;

        const auto method = tokens[0].toUpperCase();
        const auto fullPath = tokens[1];
        const auto path = fullPath.upToFirstOccurrenceOf( "?", false, false );

        // Extract body if any
        juce::String body;
        const auto headerEnd = reqStr.indexOf( "\r\n\r\n" );
        if( headerEnd >= 0 )
            body = reqStr.substring( headerEnd + 4 );

        auto sendResponse = [&]( int code, const juce::String& statusText, const juce::String& contentType, const juce::String& content ) {
            juce::String resp = "HTTP/1.1 " + juce::String( code ) + " " + statusText + "\r\n";
            resp += "Content-Type: " + contentType + "\r\n";
            resp += "Content-Length: " + juce::String( content.getNumBytesAsUTF8() ) + "\r\n";
            resp += "Access-Control-Allow-Origin: *\r\n";
            resp += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
            resp += "Access-Control-Allow-Headers: Content-Type\r\n";
            resp += "Connection: close\r\n\r\n";
            resp += content;
            const auto utf8 = resp.toRawUTF8();
            client->write( utf8, static_cast<int>( std::strlen( utf8 ) ) );
        };

        if( method == "OPTIONS" )
        {
            sendResponse( 204, "No Content", "text/plain", "" );
            return;
        }

        if( method == "GET" && ( path == "/" || path == "/index.html" ) )
        {
            sendResponse( 200, "OK", "text/html; charset=utf-8", buildHtmlPage() );
            return;
        }

        if( method == "GET" && path == "/api/status" )
        {
            StatusInfo st;
            if( getStatus )
                st = getStatus();

            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
            obj->setProperty( "google_connected", st.googleConnected );
            obj->setProperty( "google_email", st.googleEmail );
            obj->setProperty( "splice_connected", st.spliceConnected );
            obj->setProperty( "splice_user", st.spliceUser );
            obj->setProperty( "splice_folder", st.spliceFolder );
            obj->setProperty( "splice_samples", st.spliceSamples );

            sendResponse( 200, "OK", "application/json", juce::JSON::toString( juce::var( obj.get() ) ) );
            return;
        }

        if( method == "POST" && path == "/api/gdrive" )
        {
            juce::String tokenInput;
            const auto parsed = juce::JSON::parse( body );
            if( parsed.isObject() && parsed.hasProperty( "token" ) )
                tokenInput = parsed.getProperty( "token", {} ).toString();
            else
            {
                // URL-encoded form data fallback
                auto pairs = juce::StringArray::fromTokens( body, "&", "" );
                for( const auto& p : pairs )
                {
                    if( p.startsWith( "token=" ) )
                        tokenInput = juce::URL::removeEscapeChars( p.substring( 6 ) );
                }
            }

            tokenInput = tokenInput.trim();
            if( tokenInput.isEmpty() )
            {
                sendResponse( 400, "Bad Request", "application/json", "{\"ok\":false,\"error\":\"Token is empty\"}" );
                return;
            }

            if( onGoogleDriveAuth )
            {
                std::atomic<bool> done{ false };
                bool authOk = false;
                juce::String outEmail, outErr;

                onGoogleDriveAuth( tokenInput, [&]( bool ok, const juce::String& email, const juce::String& err ) {
                    authOk   = ok;
                    outEmail = email;
                    outErr   = err;
                    done     = true;
                } );

                int waitCount = 0;
                while( !done.load() && waitCount++ < 100 )
                    juce::Thread::sleep( 50 );

                if( authOk )
                    sendResponse( 200, "OK", "application/json", "{\"ok\":true,\"email\":\"" + outEmail + "\"}" );
                else
                    sendResponse( 400, "Bad Request", "application/json", "{\"ok\":false,\"error\":\"" + ( outErr.isNotEmpty() ? outErr : "Authentication failed" ) + "\"}" );
                return;
            }
            sendResponse( 500, "Server Error", "application/json", "{\"ok\":false,\"error\":\"No handler registered\"}" );
            return;
        }

        if( method == "POST" && path == "/api/splice-sync" )
        {
            if( onSpliceSync )
            {
                std::atomic<bool> done{ false };
                bool syncOk = false;
                juce::String outMsg;

                onSpliceSync( [&]( bool ok, const juce::String& msg ) {
                    syncOk = ok;
                    outMsg = msg;
                    done   = true;
                } );

                int waitCount = 0;
                while( !done.load() && waitCount++ < 200 )
                    juce::Thread::sleep( 50 );

                sendResponse( 200, "OK", "application/json", "{\"ok\":" + juce::String( syncOk ? "true" : "false" ) + ",\"message\":\"" + outMsg + "\"}" );
                return;
            }
            sendResponse( 200, "OK", "application/json", "{\"ok\":true,\"message\":\"Sync requested\"}" );
            return;
        }

        if( method == "POST" && path == "/api/upload" )
        {
            // Simple audio sample receiver
            juce::String fn = "uploaded_sample.wav";
            // Header parsing could provide filename if specified in query ?filename=...
            const auto qmark = fullPath.indexOf( "?" );
            if( qmark >= 0 )
            {
                auto qs = fullPath.substring( qmark + 1 );
                auto parts = juce::StringArray::fromTokens( qs, "&", "" );
                for( const auto& part : parts )
                {
                    if( part.startsWith( "filename=" ) )
                        fn = juce::URL::removeEscapeChars( part.substring( 9 ) );
                }
            }

            if( onSampleUpload && headerEnd >= 0 )
            {
                const size_t bodyOffset = static_cast<size_t>( headerEnd + 4 );
                if( static_cast<size_t>( bytesRead ) > bodyOffset )
                {
                    juce::MemoryBlock mb( buf.data() + bodyOffset, static_cast<size_t>( bytesRead ) - bodyOffset );
                    onSampleUpload( fn, mb );
                }
            }
            sendResponse( 200, "OK", "application/json", "{\"ok\":true,\"filename\":\"" + fn + "\"}" );
            return;
        }

        sendResponse( 404, "Not Found", "text/plain", "Not Found" );
    }

    juce::String PhonePairingServer::buildHtmlPage() const
    {
        return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <title>Kick Cloud Connect</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background: #121316;
      color: #eceff4;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      padding: 16px;
      display: flex;
      flex-direction: column;
      align-items: center;
      min-height: 100vh;
    }
    .container { width: 100%; max-width: 480px; }
    .header {
      text-align: center;
      margin-bottom: 20px;
      padding-top: 10px;
    }
    .header h1 {
      font-size: 24px;
      color: #ff6b35;
      letter-spacing: 1px;
      margin-bottom: 4px;
    }
    .header p { font-size: 13px; color: #888ea8; }
    .card {
      background: #1e2026;
      border: 1px solid #2e323e;
      border-radius: 14px;
      padding: 20px;
      margin-bottom: 18px;
      box-shadow: 0 4px 20px rgba(0,0,0,0.4);
    }
    .card h2 {
      font-size: 18px;
      color: #4dabf7;
      margin-bottom: 12px;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .card p { font-size: 13px; color: #a0a6b8; line-height: 1.5; margin-bottom: 12px; }
    .badge {
      display: inline-block;
      padding: 4px 10px;
      border-radius: 20px;
      font-size: 12px;
      font-weight: 600;
      margin-bottom: 14px;
    }
    .badge-ok { background: #2b8a3e; color: #fff; }
    .badge-off { background: #495057; color: #adb5bd; }
    input[type="text"], textarea {
      width: 100%;
      background: #14161b;
      border: 1px solid #363a49;
      border-radius: 8px;
      color: #fff;
      padding: 12px;
      font-size: 14px;
      margin-bottom: 12px;
      outline: none;
      transition: border 0.2s;
    }
    input[type="text"]:focus, textarea:focus { border-color: #4dabf7; }
    button {
      width: 100%;
      background: #ff6b35;
      color: #fff;
      border: none;
      border-radius: 8px;
      padding: 14px;
      font-size: 15px;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.2s, transform 0.1s;
    }
    button:active { transform: scale(0.98); }
    button:hover { background: #ff8555; }
    .btn-secondary {
      background: #2b303c;
      color: #ced4da;
      margin-top: 8px;
    }
    .btn-secondary:hover { background: #383e4d; }
    .btn-sync { background: #37b24d; }
    .btn-sync:hover { background: #40c057; }
    .link-helper {
      display: block;
      text-align: right;
      font-size: 12px;
      color: #4dabf7;
      text-decoration: none;
      margin-top: -6px;
      margin-bottom: 12px;
    }
    .link-helper:hover { text-decoration: underline; }
    .alert {
      padding: 10px 14px;
      border-radius: 8px;
      font-size: 13px;
      margin-top: 12px;
      display: none;
    }
    .alert-success { background: rgba(43, 138, 62, 0.25); border: 1px solid #2b8a3e; color: #8ce99a; }
    .alert-error { background: rgba(224, 49, 49, 0.25); border: 1px solid #e03131; color: #ff8787; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>⚡ KICK CLOUD CONNECT</h1>
      <p>Connect Google Drive & Splice directly to your Kick DAW</p>
    </div>

    <!-- Google Drive Card -->
    <div class="card">
      <h2>📁 Google Drive</h2>
      <div id="gdriveBadge" class="badge badge-off">Checking status...</div>
      <p>Connect your Google Drive account. Your cloud samples, loops, and projects will appear in Kick File Browser.</p>

      <a class="link-helper" href="https://developers.google.com/oauthplayground" target="_blank">Get Token via Google Playground ↗</a>
      <input type="text" id="gdriveToken" placeholder="Paste Google OAuth Token (ya29... / 1//...) or Client ID">
      <button id="btnConnectGdrive" onclick="connectGoogleDrive()">Connect to Kick</button>
      <div id="gdriveAlert" class="alert"></div>
    </div>

    <!-- Splice Card -->
    <div class="card">
      <h2>🎵 Splice Sound Sync</h2>
      <div id="spliceBadge" class="badge badge-off">Checking status...</div>
      <p id="spliceInfo">Splice samples sync directly to your device storage.</p>
      <button class="btn-sync" id="btnSyncSplice" onclick="syncSplice()">Sync Samples Now</button>
      <button class="btn-secondary" onclick="window.open('https://splice.com/link','_blank')">Link Splice Account ↗</button>
      <div id="spliceAlert" class="alert"></div>
    </div>
  </div>

  <script>
    async function refreshStatus() {
      try {
        const res = await fetch('/api/status');
        if (!res.ok) return;
        const st = await res.json();

        const gdBadge = document.getElementById('gdriveBadge');
        if (st.google_connected && st.google_email) {
          gdBadge.className = 'badge badge-ok';
          gdBadge.textContent = 'Connected: ' + st.google_email;
        } else {
          gdBadge.className = 'badge badge-off';
          gdBadge.textContent = 'Not Connected';
        }

        const spBadge = document.getElementById('spliceBadge');
        const spInfo = document.getElementById('spliceInfo');
        if (st.splice_connected) {
          spBadge.className = 'badge badge-ok';
          spBadge.textContent = 'Connected' + (st.splice_user ? ' (' + st.splice_user + ')' : '');
          if (st.splice_folder) {
            spInfo.textContent = 'Folder: ' + st.splice_folder + (st.splice_samples > 0 ? ' (' + st.splice_samples + ' samples)' : '');
          }
        } else {
          spBadge.className = 'badge badge-off';
          spBadge.textContent = 'Not Linked';
        }
      } catch (e) {
        console.error(e);
      }
    }

    async function connectGoogleDrive() {
      const input = document.getElementById('gdriveToken');
      const token = input.value.trim();
      const alertBox = document.getElementById('gdriveAlert');
      const btn = document.getElementById('btnConnectGdrive');

      if (!token) {
        alertBox.className = 'alert alert-error';
        alertBox.textContent = 'Please paste a token or Client ID.';
        alertBox.style.display = 'block';
        return;
      }

      btn.disabled = true;
      btn.textContent = 'Connecting...';
      alertBox.style.display = 'none';

      try {
        const res = await fetch('/api/gdrive', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ token: token })
        });
        const data = await res.json();
        if (data.ok) {
          alertBox.className = 'alert alert-success';
          alertBox.textContent = 'Connected! Google Drive (' + (data.email || 'Ready') + ') is now mapped in Kick.';
          alertBox.style.display = 'block';
          input.value = '';
          refreshStatus();
        } else {
          alertBox.className = 'alert alert-error';
          alertBox.textContent = 'Error: ' + (data.error || 'Connection failed');
          alertBox.style.display = 'block';
        }
      } catch (e) {
        alertBox.className = 'alert alert-error';
        alertBox.textContent = 'Network error: ' + e.message;
        alertBox.style.display = 'block';
      } finally {
        btn.disabled = false;
        btn.textContent = 'Connect to Kick';
      }
    }

    async function syncSplice() {
      const alertBox = document.getElementById('spliceAlert');
      const btn = document.getElementById('btnSyncSplice');
      btn.disabled = true;
      btn.textContent = 'Syncing...';
      alertBox.style.display = 'none';

      try {
        const res = await fetch('/api/splice-sync', { method: 'POST' });
        const data = await res.json();
        alertBox.className = 'alert alert-success';
        alertBox.textContent = data.message || 'Splice sync triggered successfully!';
        alertBox.style.display = 'block';
        refreshStatus();
      } catch (e) {
        alertBox.className = 'alert alert-error';
        alertBox.textContent = 'Sync error: ' + e.message;
        alertBox.style.display = 'block';
      } finally {
        btn.disabled = false;
        btn.textContent = 'Sync Samples Now';
      }
    }

    refreshStatus();
    setInterval(refreshStatus, 4000);
  </script>
</body>
</html>
)HTML";
    }
}
