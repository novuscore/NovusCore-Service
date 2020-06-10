#include "EngineLoop.h"
#include <thread>
#include <iostream>
#include <Utils/Timer.h>
#include "Utils/ServiceLocator.h"
#include "Utils/EntityUtils.h"
#include <Networking/InputQueue.h>
#include <Networking/MessageHandler.h>
#include <Networking/NetworkClient.h>
#include <tracy/Tracy.hpp>

// Component Singletons
#include "ECS/Components/Singletons/TimeSingleton.h"
#include "ECS/Components/Network/ConnectionDeferredSingleton.h"

// Components
#include "ECS/Components/Network/ConnectionComponent.h"
#include "ECS/Components/GameEntityInfo.h"
#include "ECS/Components/Transform.h"

// Systems
#include "ECS/Systems/Network/ConnectionSystems.h"
#include "ECS/Systems/Network/InitializePlayerSystem.h"

// Handlers
#include "Network/Handlers/Server/GeneralHandlers.h"

EngineLoop::EngineLoop()
    : _isRunning(false), _inputQueue(256), _outputQueue(256)
{
    _network.asioService = std::make_shared<asio::io_service>(2);
    _network.server = std::make_shared<NetworkServer>(_network.asioService, 3724);

    /* Example Usage of our current DB Interface
    
    std::shared_ptr<QueryResult> result = conn.Query("SELECT id, value, date FROM test");
    while (result->GetNextRow())
    {
        const Field& idField = result->GetField(0);
        const Field& valueField = result->GetField(1);
        const Field& dateField = result->GetField(2);
    
        //NC_LOG_SUCCESS("Query data(%i, %f, %s)", idField.GetI32(), valueField.GetF32(), dateField.GetString().c_str());
    }*/
}

EngineLoop::~EngineLoop()
{
}

void EngineLoop::Start()
{
    if (_isRunning)
        return;

    // Setup Input Queue for libraries
    InputQueue::SetInputQueue(&_inputQueue);

    std::thread threadRun = std::thread(&EngineLoop::Run, this);
    std::thread threadRunIoService = std::thread(&EngineLoop::RunIoService, this);
    threadRun.detach();
    threadRunIoService.detach();
}

void EngineLoop::Stop()
{
    if (!_isRunning)
        return;

    Message message;
    message.code = MSG_IN_EXIT;
    PassMessage(message);
}

void EngineLoop::PassMessage(Message& message)
{
    _inputQueue.enqueue(message);
}

bool EngineLoop::TryGetMessage(Message& message)
{
    return _outputQueue.try_dequeue(message);
}

void EngineLoop::Run()
{
    _isRunning = true;

    SetupUpdateFramework();
    _updateFramework.gameRegistry.create();

    TimeSingleton& timeSingleton = _updateFramework.gameRegistry.set<TimeSingleton>();
    ConnectionDeferredSingleton& connectionDeferredSingleton = _updateFramework.gameRegistry.set<ConnectionDeferredSingleton>();
    connectionDeferredSingleton.networkServer = _network.server;

    _network.server->SetConnectionHandler(std::bind(&ConnectionUpdateSystem::HandleConnection, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    _network.server->Start();

    /*DBConnection conn("localhost", 3306, "root", "ascent", "novuscore", 0, 2);
    
    for (i32 i = 0; i < 11; i++)
    {
        conn.QueryAsync("SELECT id, value, date FROM test",
            [&](std::shared_ptr<QueryResult> result)
            {
                while (result->GetNextRow())
                {
                    const Field& idField = result->GetField(0);
                    const Field& valueField = result->GetField(1);
                    const Field& dateField = result->GetField(2);

                    PrintMessage("WorkerThread completed query");
                }
            });
    }*/

    // Create New Entity
    /*entt::entity entity = _updateFramework.gameRegistry.create();

    GameEntityInfo& gameEntityInfo = _updateFramework.gameRegistry.assign<GameEntityInfo>(entity);
    gameEntityInfo.type = GameEntityType::GAMEOBJECT;
    gameEntityInfo.entryId = 1;

    Transform& transform = _updateFramework.gameRegistry.assign<Transform>(entity);
    transform.position = vec3(16533.3320f, 0.0f, 26133.3320f);
    transform.rotation = vec3(0, 0, 0);
    transform.scale = vec3(1, 1, 1);

    entt::entity entity1 = _updateFramework.gameRegistry.create();

    GameEntityInfo& gameEntityInfo1 = _updateFramework.gameRegistry.assign<GameEntityInfo>(entity1);
    gameEntityInfo1.type = GameEntityType::GAMEOBJECT;
    gameEntityInfo1.entryId = 1;

    Transform& transform1 = _updateFramework.gameRegistry.assign<Transform>(entity1);
    transform1.position = vec3(16543.3320f, 0.0f, 26143.3320f);
    transform1.rotation = vec3(0, 0, 0);
    transform1.scale = vec3(1, 1, 1);*/

    Timer timer;
    f32 targetDelta = 1.0f / 60.0f;
    while (true)
    {
        f32 deltaTime = timer.GetDeltaTime();
        timer.Tick();

        timeSingleton.lifeTimeInS = timer.GetLifeTime();
        timeSingleton.lifeTimeInMS = timeSingleton.lifeTimeInS * 1000;
        timeSingleton.deltaTime = deltaTime;

        if (!Update())
            break;

        {
            ZoneScopedNC("WaitForTickRate", tracy::Color::AntiqueWhite1)

            // Wait for tick rate, this might be an overkill implementation but it has the even tickrate I've seen - MPursche
            {
                ZoneScopedNC("Sleep", tracy::Color::AntiqueWhite1) for (deltaTime = timer.GetDeltaTime(); deltaTime < targetDelta - 0.0025f; deltaTime = timer.GetDeltaTime())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            {
                ZoneScopedNC("Yield", tracy::Color::AntiqueWhite1) for (deltaTime = timer.GetDeltaTime(); deltaTime < targetDelta; deltaTime = timer.GetDeltaTime())
                {
                    std::this_thread::yield();
                }
            }
        }


        FrameMark
    }

    // Clean up stuff here

    Message exitMessage;
    exitMessage.code = MSG_OUT_EXIT_CONFIRM;
    _outputQueue.enqueue(exitMessage);
}
void EngineLoop::RunIoService()
{
    asio::io_service::work ioWork(*_network.asioService.get());
    _network.asioService->run();
}

bool EngineLoop::Update()
{
    ZoneScopedNC("Update", tracy::Color::Blue2)
    {
        ZoneScopedNC("HandleMessages", tracy::Color::Green3)
            Message message;

        while (_inputQueue.try_dequeue(message))
        {
            if (message.code == -1)
                assert(false);

            if (message.code == MSG_IN_EXIT)
            {
                return false;
            }
            else if (message.code == MSG_IN_PING)
            {
                ZoneScopedNC("Ping", tracy::Color::Green3)
                    Message pongMessage;
                pongMessage.code = MSG_OUT_PRINT;
                pongMessage.message = new std::string("PONG!");
                _outputQueue.enqueue(pongMessage);
            }
        }
    }

    UpdateSystems();
    return true;
}

void EngineLoop::SetupUpdateFramework()
{
    tf::Framework& framework = _updateFramework.framework;
    entt::registry& gameRegistry = _updateFramework.gameRegistry;

    ServiceLocator::SetGameRegistry(&gameRegistry);
    SetMessageHandler();

    // @TODO: Temporary fix to allow taskflow to run multiple tasks at the same time when using Entt to construct views
    gameRegistry.prepare<ConnectionComponent>();

    // ConnectionUpdateSystem
    tf::Task connectionUpdateSystemTask = framework.emplace([&gameRegistry]()
    {
        ZoneScopedNC("ConnectionUpdateSystem::Update", tracy::Color::Blue2)
        ConnectionUpdateSystem::Update(gameRegistry);
    });

    // InitializePlayerSystem
    tf::Task initializePlayerSystemTask = framework.emplace([&gameRegistry]()
    {
        ZoneScopedNC("InitializePlayerSystem::Update", tracy::Color::Blue2)
            InitializePlayerSystem::Update(gameRegistry);
    });
    initializePlayerSystemTask.gather(connectionUpdateSystemTask);

    // ConnectionDeferredSystem
    tf::Task connectionDeferredSystemTask = framework.emplace([&gameRegistry]()
    {
        ZoneScopedNC("ConnectionDeferredSystem::Update", tracy::Color::Blue2)
            ConnectionDeferredSystem::Update(gameRegistry);
    });
    connectionDeferredSystemTask.gather(initializePlayerSystemTask);
}
void EngineLoop::SetMessageHandler()
{
    auto messageHandler = new MessageHandler();
    ServiceLocator::SetNetworkMessageHandler(messageHandler);

    Server::GeneralHandlers::Setup(messageHandler);
}
void EngineLoop::UpdateSystems()
{
    ZoneScopedNC("UpdateSystems", tracy::Color::Blue2)
    {
        ZoneScopedNC("Taskflow::Run", tracy::Color::Blue2)
            _updateFramework.taskflow.run(_updateFramework.framework);
    }
    {
        ZoneScopedNC("Taskflow::WaitForAll", tracy::Color::Blue2)
            _updateFramework.taskflow.wait_for_all();
    }
}
