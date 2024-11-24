#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/netanim-module.h"
#include <netinet/in.h>

 
using namespace ns3;
#define RSS_VALUE (-70.0)
#define NUM_NODES 2
 
NS_LOG_COMPONENT_DEFINE("teste");

class MyApp : public Application
{
public:
    MyApp ();
    virtual ~MyApp ();

    static TypeId GetTypeId (void);
    void Setup (int index,Ptr<Socket> sender_socket,Ptr<Socket> receiver_socket,Ipv4Address right_neighbor_address,Ipv4Address left_neighbor_address,bool is_edge);

    virtual void StartApplication (void);
    void OnAccept (Ptr<Socket> s, const Address& from);
    void OnReceive (Ptr<Socket> socket);
    void Connect (Ipv4Address neighbor_address);
    void ConnectionSucceeded(Ptr<Socket> socket);
    void ConnectionFailed(Ptr<Socket> socket);

    void SendPacket (int32_t number);

    int index;
    Ptr<Socket> sender_socket;
    uint16_t receiver_port = 8080;
    Ptr<Socket> receiver_socket;
    bool is_running;
    bool is_edge;
    Ipv4Address right_neighbor_address;
    Ipv4Address left_neighbor_address;
};

MyApp::MyApp ()
    : sender_socket (0),
    receiver_socket (0),
    is_running (false),
    is_edge (false)
{
}

MyApp::~MyApp ()
{
    sender_socket = 0;
    receiver_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId (void)
{
    static TypeId tid = TypeId ("MyApp")
    .SetParent<Application> ()
    .AddConstructor<MyApp> ()
    ;
    return tid;
}

void
MyApp::Setup (int index,Ptr<Socket> sender_socket,Ptr<Socket> receiver_socket,Ipv4Address right_neighbor_address,Ipv4Address left_neighbor_address,bool is_edge = false)
{
    this->index = index;
    this->sender_socket = sender_socket;
    this->receiver_socket = receiver_socket;
    this->right_neighbor_address = right_neighbor_address;
    this->left_neighbor_address = left_neighbor_address;
    this->is_edge = is_edge;
}

void
MyApp::StartApplication (void)
{
    is_running = true;
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), receiver_port);
    if (receiver_socket->Bind(local) == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }
    receiver_socket->Listen();
    receiver_socket->SetAcceptCallback(
      MakeNullCallback<bool, Ptr<Socket>, const Address&> (),
      MakeCallback(&MyApp::OnAccept, this)
    );
}

void MyApp::OnAccept(Ptr<Socket> s, const Address& from) {
    s->SetRecvCallback(MakeCallback(&MyApp::OnReceive, this));
}

void MyApp::OnReceive(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet;
    int32_t networkOrderNumber;
    int32_t receivedNumber = 0;
    // Continuar escutando infinitamente
    while (packet = socket->RecvFrom(from)) {
        InetSocketAddress inetFrom = InetSocketAddress::ConvertFrom(from);
        packet->CopyData((uint8_t *)&networkOrderNumber, sizeof(networkOrderNumber));
        receivedNumber = ntohl(networkOrderNumber);

        // Lógica para processamento do pacote
        if (this->is_edge) {
            // Gera um número aleatório quando for uma borda
            receivedNumber = 1 + rand() % 100;
            Connect(this->left_neighbor_address);
        }
        else {
            if (this->right_neighbor_address == inetFrom.GetIpv4()) {
                Connect(this->left_neighbor_address);
            }
            else {
                Connect(this->right_neighbor_address);
            }
        }
        
        // Enviar o pacote processado
        SendPacket(receivedNumber);
    }
}

void
MyApp::SendPacket (int32_t number)
{
    int32_t networkOrderNumber = htonl(number);
    Ptr<Packet> packet = Create<Packet>((uint8_t *)&networkOrderNumber, sizeof(networkOrderNumber));
    sender_socket->Send(packet);
    NS_LOG_UNCOND("nó "<< this->index << " manda " << number);
    sender_socket->Close();
}

void
MyApp::Connect (Ipv4Address neighbor_address)
{
    sender_socket->SetConnectCallback (
        MakeCallback(&MyApp::ConnectionSucceeded, this),
        MakeCallback(&MyApp::ConnectionFailed, this)
    );
    InetSocketAddress remote = InetSocketAddress(neighbor_address, this->receiver_port);
    sender_socket->Connect(remote);
    NS_LOG_UNCOND("nó "<< this->index << " conecta com " << neighbor_address);
}

// Callback para conexão bem-sucedida
void MyApp::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_UNCOND("Conexão bem-sucedida");
}

// Callback para falha de conexão
void MyApp::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_UNCOND("Falha na conexão");
}

void SendSinglePacket(Ptr<Socket> sender_socket)
{
    int32_t networkOrderNumber = htonl(1+rand()%100);
    Ptr<Packet> packet = Create<Packet>((uint8_t *)&networkOrderNumber, sizeof(networkOrderNumber));
    sender_socket->Send(packet);
    NS_LOG_UNCOND("nó 0 manda" << 1+rand()%100 <<"para nó 1");
    sender_socket->Close();
}

void ConnectAfterReceiverStart(Ptr<Socket> sender_socket, Ipv4Address receiver_address, uint16_t port)
{
    InetSocketAddress remote = InetSocketAddress(receiver_address, port);
    sender_socket->Connect(remote);
    NS_LOG_UNCOND("nó 0 connecta com nó 1");
}



int
main(int argc, char* argv[])
{
    Time::SetResolution(Time::NS);
    NodeContainer nodes;
    nodes.Create(NUM_NODES);

    std::string phyMode ("DsssRate1Mbps");

    /* Wifi Stuff*/
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set ("RxGain", DoubleValue (0) );
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (RSS_VALUE));
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
    // Set it to adhoc mode
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install (wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    // positionAlloc->Add(Vector(10.0, 0.0, 0.0));
    // positionAlloc->Add(Vector(15.0, 0.0, 0.0));
    // positionAlloc->Add(Vector(20.0, 0.0, 0.0));
    
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);


    // criação dos sockets e aplicações
    for( int i = 0; i < NUM_NODES; i++ ){
        //criando socket de recebimento 1
        Ptr<Socket> receiver_socket = Socket::CreateSocket (nodes.Get (i), TcpSocketFactory::GetTypeId ());

        //criando socket de envio 1
        Ptr<Socket> sender_socket = Socket::CreateSocket (nodes.Get (i), TcpSocketFactory::GetTypeId ());

        if ( i == 0 ){
            //nó inicial
            Ptr<MyApp> app = CreateObject<MyApp> ();
            app->Setup (i,sender_socket, receiver_socket,interfaces.GetAddress(i+1),interfaces.GetAddress(i+1),true);
            nodes.Get (i)->AddApplication (app);
            app->SetStartTime (Seconds (1.));
            app->SetStopTime (Seconds (30.));
            continue;
        }
        if ( i == NUM_NODES - 1 ){
            //nó final
            Ptr<MyApp> app = CreateObject<MyApp> ();
            app->Setup (i,sender_socket, receiver_socket,interfaces.GetAddress(i-1),interfaces.GetAddress(i-1),true);
            nodes.Get (i)->AddApplication (app);
            app->SetStartTime (Seconds (1.));
            app->SetStopTime (Seconds (30.));
            continue;
        }
        //criando aplicação
        Ptr<MyApp> app = CreateObject<MyApp> ();
        app->Setup (i,sender_socket, receiver_socket,interfaces.GetAddress(i+1),interfaces.GetAddress(i-1),false);
        nodes.Get (i)->AddApplication (app);
        app->SetStartTime (Seconds (1.));
        app->SetStopTime (Seconds (30.));

    }

    // NS_LOG_UNCOND("nó 0 : " << interfaces.GetAddress(0));
    // NS_LOG_UNCOND("nó 1 : " << interfaces.GetAddress(1));
    // NS_LOG_UNCOND("nó 2 : " << interfaces.GetAddress(2));
    // NS_LOG_UNCOND("nó 3 : " << interfaces.GetAddress(3));
    // NS_LOG_UNCOND("nó 4 : " << interfaces.GetAddress(4));
    
    // Configurando o nó de envio (nó 0)
    Ptr<Socket> sender_socket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
    // Agendar a conexão e o envio após o receptor iniciar
    // Primeiro envio para começar a cadeia
    Simulator::Schedule(Seconds(1.0), &ConnectAfterReceiverStart, sender_socket, interfaces.GetAddress(0), 8080);
    Simulator::Schedule(Seconds(1.1), &SendSinglePacket, sender_socket);

    //LogComponentEnable("Socket", LOG_LEVEL_ALL);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    // Inicializar o NetAnim
    AnimationInterface anim("simulation.xml"); // Arquivo de saída da animação
    
    Time stop_time = Time("50s");
    Simulator::Stop(stop_time);
    Simulator::Run();
    Simulator::Destroy();
 
    return 0;
}