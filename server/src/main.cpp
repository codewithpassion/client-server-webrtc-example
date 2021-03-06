// This a minimal fully functional example for setting up a server written in C++ that communicates
// with clients via WebRTC data channels. This uses WebSockets to perform the WebRTC handshake
// (offer/accept SDP) with the client. We only use WebSockets for the initial handshake because TCP
// often presents too much latency in the context of real-time action games. WebRTC data channels,
// on the other hand, allow for unreliable and unordered message sending via SCTP.
//
// Author: brian@brkho.com

#include "observers.h"

#include "modules/audio_device/include/fake_audio_device.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include <rtc_base/physical_socket_server.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include "modules/audio_device/include/fake_audio_device.h"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <iostream>
#include <thread>

#include <nlohmann/json.hpp>

// WebSocket++ types are gnarly.
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// Some forward declarations.
void OnDataChannelCreated(rtc::scoped_refptr<webrtc::DataChannelInterface> channel);
void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
void OnDataChannelMessage(const webrtc::DataBuffer& buffer);
void OnAnswerCreated(webrtc::SessionDescriptionInterface* desc);

typedef websocketpp::server<websocketpp::config::asio> WebSocketServer;
typedef WebSocketServer::message_ptr message_ptr;

// The WebSocket server being used to handshake with the clients.
WebSocketServer ws_server;
// The peer conncetion factory that sets up signaling and worker threads. It is also used to create
// the PeerConnection.
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
// The socket that the signaling thread and worker thread communicate on.
rtc::PhysicalSocketServer socket_server;
// The separate thread where all of the WebRTC code runs since we use the main thread for the
// WebSocket listening loop.
std::thread webrtc_thread;
// The WebSocket connection handler that uniquely identifies one of the connections that the
// WebSocket has open. If you want to have multiple connections, you will need to store more than
// one of these.
websocketpp::connection_hdl websocket_connection_handler;
// The peer connection through which we engage in the SDP handshake.
rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection;
// The data channel used to communicate.
rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel;
// The observer that responds to peer connection events.
PeerConnectionObserver peer_connection_observer(OnDataChannelCreated, OnIceCandidate);
// The observer that responds to data channel events.
DataChannelObserver data_channel_observer(OnDataChannelMessage);
// The observer that responds to session description creation events.
CreateSessionDescriptionObserver create_session_description_observer(OnAnswerCreated);
// The observer that responds to session description set events. We don't really use this one here.
SetSessionDescriptionObserver set_session_description_observer;

std::unique_ptr<rtc::Thread> workerThread;
std::unique_ptr<rtc::Thread> signalingThread;


// Callback for when the data channel is successfully created. We need to re-register the updated
// data channel here.
void OnDataChannelCreated(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
  data_channel = channel;
  data_channel->RegisterObserver(&data_channel_observer);
}

// Callback for when the STUN server responds with the ICE candidates.
void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
  std::string candidate_str;
  candidate->ToString(&candidate_str);

  nlohmann::json iceCandidateMsg = {
          { "type", "candidate" },
          { "payload", {
               { "candidate",       candidate_str.c_str() },
               { "sdpMid",          candidate->sdp_mid().c_str() },
               { "sdpMLineIndex",   candidate->sdp_mline_index() }
          }}
  };
  ws_server.send(websocket_connection_handler, iceCandidateMsg.dump(), websocketpp::frame::opcode::value::text);
}

// Callback for when the server receives a message on the data channel.
void OnDataChannelMessage(const webrtc::DataBuffer& buffer) {
   std::string data(buffer.data.data<char>(), buffer.data.size());
   std::cout << "Message: " << data << std::endl;
   std::string str = "pong: " + data;
   webrtc::DataBuffer resp(rtc::CopyOnWriteBuffer(str.c_str(), str.length()), false /* binary */);
    data_channel->Send(resp);
}


// Callback for when the answer is created. This sends the answer back to the client.
void OnAnswerCreated(webrtc::SessionDescriptionInterface* desc) {
    peer_connection->SetLocalDescription(&set_session_description_observer, desc);
    // Apologies for the poor code ergonomics here; I think rapidjson is just verbose.
    std::string offer_string;
    desc->ToString(&offer_string);

    nlohmann::json answer = {
            { "type", "answer" },
            { "payload", {
              { "type", "answer" },
              { "sdp" , offer_string.c_str() }
            }}
    };

    ws_server.send(websocket_connection_handler, answer.dump(), websocketpp::frame::opcode::value::text);
}

// Callback for when the WebSocket server receives a message from the client.
void OnWebSocketMessage(WebSocketServer* /* s */, websocketpp::connection_hdl hdl, message_ptr msg) {
  websocket_connection_handler = hdl;

  nlohmann::json message = nlohmann::json::parse(msg->get_payload());

  // Probably should do some error checking on the JSON object.
  std::string type = message["type"];


  if (type == "ping") {
    std::string id = msg->get_payload().c_str();
    ws_server.send(websocket_connection_handler, id, websocketpp::frame::opcode::value::text);

  }
  else if (type == "offer") {

    std::string sdp = message["payload"]["sdp"];

    std::cerr << "offer sdp: " << sdp << std::endl;

    webrtc::PeerConnectionInterface::RTCConfiguration configuration;

    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:stun1.l.google.com:19302";

    webrtc::PeerConnectionInterface::IceServer ice_server2;
    ice_server2.uri = "stun:stun2.l.google.com:19305";

    configuration.servers.push_back(ice_server);
    configuration.servers.push_back(ice_server2);

    auto* obs = &peer_connection_observer;

    peer_connection = peer_connection_factory->CreatePeerConnection(
        configuration,
        nullptr,
        nullptr,
        obs);

    webrtc::DataChannelInit data_channel_config;
    data_channel_config.ordered = false;
    data_channel_config.maxRetransmits = 0;
    data_channel = peer_connection->CreateDataChannel("dc", &data_channel_config);
    data_channel->RegisterObserver(&data_channel_observer);

    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface* session_description(
        webrtc::CreateSessionDescription("offer", sdp, &error));

    peer_connection->SetRemoteDescription(&set_session_description_observer, session_description);

    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    peer_connection->CreateAnswer(&create_session_description_observer, options);

  } else if (type == "candidate") {
    std::string candidate = message["payload"]["candidate"];
    int sdp_mline_index = message["payload"]["sdpMLineIndex"].get<int>();
    std::string sdp_mid = message["payload"]["sdpMid"];
    webrtc::SdpParseError error;

    auto candidate_object = webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, candidate, &error);
    peer_connection->AddIceCandidate(candidate_object);
  } else {
    std::cout << "Unrecognized WebSocket message type." << std::endl;
  }
}

// The thread entry point for the WebRTC thread. This sets the WebRTC thread as the signaling thread
// and creates a worker thread in the background.
void SignalThreadEntry() {
  // Create the PeerConnectionFactory.
  rtc::InitializeSSL();

  workerThread = rtc::Thread::CreateWithSocketServer();
  signalingThread = rtc::Thread::Create();

  try {

      if (!signalingThread->Start() || !workerThread->Start())
      {
          throw std::runtime_error("thread start errored");
      }

      peer_connection_factory = webrtc::CreatePeerConnectionFactory(
              workerThread.get(),
              workerThread.get(),
              signalingThread.get(),
              new webrtc::FakeAudioDeviceModule(),//nullptr /*default_adm*/,
              webrtc::CreateBuiltinAudioEncoderFactory(),
              webrtc::CreateBuiltinAudioDecoderFactory(),
              webrtc::CreateBuiltinVideoEncoderFactory(),
              webrtc::CreateBuiltinVideoDecoderFactory(),
              nullptr /*audio_mixer*/,
              nullptr /*audio_processing*/);

  }
  catch (std::exception& e) {
    std::cerr << ">>> Error: " << e.what() << std::endl;
  }
}

// Main entry point of the code.
int main() {
//  webrtc_thread = std::thread(SignalThreadEntry);

    SignalThreadEntry();

  // In a real game server, you would run the WebSocket server as a separate thread so your main
  // process can handle the game loop.
  ws_server.set_message_handler(bind(OnWebSocketMessage, &ws_server, ::_1, ::_2));
  ws_server.init_asio();
  ws_server.clear_access_channels(websocketpp::log::alevel::all);
  ws_server.set_reuse_addr(true);
  ws_server.listen(8080);
  ws_server.start_accept();
  // I don't do it here, but you should gracefully handle closing the connection.
  ws_server.run();
  rtc::CleanupSSL();
}
