#include <iostream>
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

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include "observers.h"

#include <nlohmann/json.hpp>
#include <thread>

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// Forward def
void OnDataChannelCreated(rtc::scoped_refptr<webrtc::DataChannelInterface> channel);
void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
void OnCreateSessionDescription(webrtc::SessionDescriptionInterface* sessionDescription);
void OnDataChannelMessage(const webrtc::DataBuffer& buffer);


// WebRTC things
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection;

std::unique_ptr<rtc::Thread> workerThread;
std::unique_ptr<rtc::Thread> signalingThread;

rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel;

// Websocket things
// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;
client::connection_ptr webSocket;
websocketpp::connection_hdl websocket_connection_handler;

// Webrtc observers
PeerConnectionObserver peer_connection_observer(OnDataChannelCreated, OnIceCandidate);
CreateSessionDescriptionObserver create_session_desc_observer(OnCreateSessionDescription);
SetSessionDescriptionObserver    setSessionDescription;
// The observer that responds to session description set events. We don't really use this one here.
SetSessionDescriptionObserver set_session_description_observer;
DataChannelObserver data_channel_observer(OnDataChannelMessage);


void OnDataChannelMessage(const webrtc::DataBuffer& buffer) {
    std::string data(buffer.data.data<char>(), buffer.data.size());
    std::cout << "### Got message: " << data << std::endl;
}

void OnCreateSessionDescription(webrtc::SessionDescriptionInterface* sessionDescription) {
    peer_connection->SetLocalDescription(&setSessionDescription, sessionDescription);

    std::string offer_string;
    sessionDescription->ToString(&offer_string);
    nlohmann::json offer {
            { "type", "offer" },
            { "payload", {
                    { "type", "offer" },
                    { "sdp",  offer_string }
            } }
    };

    webSocket->send(offer.dump(), websocketpp::frame::opcode::value::text);
}

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
                              { "candidate",       candidate_str },
                              { "sdpMid",          candidate->sdp_mid() },
                              { "sdpMLineIndex",   candidate->sdp_mline_index() }
                      }}
    };
    webSocket->send(iceCandidateMsg.dump(), websocketpp::frame::opcode::value::text);
}

// This message handler will be invoked once for each incoming message. It
// prints the message and then sends a copy of the message back to the server.
void on_message(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
    std::cout << "on_message called with hdl: " << hdl.lock().get()
              << " and message: " << msg->get_payload()
              << std::endl;

    nlohmann::json message = nlohmann::json::parse(msg->get_payload());

    if (message["type"] == "answer") {

        webrtc::SdpParseError error;
        auto payload = message["payload"];

        webrtc::SessionDescriptionInterface* session_description(
                webrtc::CreateSessionDescription("answer", payload["sdp"], &error));

        peer_connection->SetRemoteDescription(&set_session_description_observer, session_description);
    }
    else if (message["type"] == "candidate") {
        auto payload = message["payload"];

        webrtc::SdpParseError error;
        auto candidate_object = webrtc::CreateIceCandidate(payload["sdpMid"], payload["sdpMLineIndex"], payload["candidate"], &error);
        peer_connection->AddIceCandidate(candidate_object);

    }
}

void on_open(websocketpp::connection_hdl hndl) {
    websocket_connection_handler = hndl;

    std::cout << "Connected to websocket" << std::endl;

    webrtc::PeerConnectionInterface::RTCConfiguration configuration;

    webrtc::PeerConnectionInterface::IceServer ice_server;
    webrtc::PeerConnectionInterface::IceServer ice_server2;
    ice_server.uri = "stun:stun1.l.google.com:19302";
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

    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = false;
    options.offer_to_receive_video = false;
    peer_connection->CreateOffer(&create_session_desc_observer, options);

}

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


int main() {
    SignalThreadEntry();

    client c;

    std::string uri = "ws://localhost:8080";
    // Set logging to be pretty verbose (everything except message payloads)
    c.set_access_channels(websocketpp::log::alevel::all);
    c.clear_access_channels(websocketpp::log::alevel::frame_payload);

    // Initialize ASIO
    c.init_asio();

    // Register our message handler
    c.set_message_handler(bind(&on_message,&c,::_1,::_2));
    c.set_open_handler([&](auto hndl) { on_open(hndl); });

    websocketpp::lib::error_code ec;
    webSocket = c.get_connection(uri, ec);
    if (ec) {
        std::cout << "could not create connection because: " << ec.message() << std::endl;
        return 0;
    }

    // Note that connect here only requests a connection. No network messages are
    // exchanged until the event loop starts running in the next line.
    c.connect(webSocket);

    std::thread r([&](){
        c.run();
    });
    std::cout << "Type a message and hit <enter>";
    for (std::string line; std::getline(std::cin, line);) {

        if (line == "quit") {
            c.stop();
            break;
        } else {
            webrtc::DataBuffer data(rtc::CopyOnWriteBuffer(line.c_str(), line.length()), false /* binary */);
            data_channel->Send(data);
        }

    }

    peer_connection_factory->Release();

    workerThread->Stop();
    signalingThread->Stop();
    rtc::CleanupSSL();

    return 0;
}