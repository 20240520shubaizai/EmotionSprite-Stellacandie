cmake_minimum_required(VERSION 3.21)

project(EmotionSprite VERSION 0.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Network Quick Qml Sql Widgets Test)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(EmotionSprite
    assets/app/Stellacandie.rc
    src/main.cpp
    src/AppController.cpp
    src/AppController.h
    src/ChatMessageModel.cpp
    src/ChatMessageModel.h
    src/StorageService.cpp
    src/StorageService.h
    src/data/SchemaMigrator.cpp
    src/data/SchemaMigrator.h
    src/agent/AgentClient.cpp
    src/agent/AgentClient.h
    src/agent/ChatAgentAdapter.cpp
    src/agent/ChatAgentAdapter.h
    src/agent/AgentServiceSupervisor.cpp
    src/agent/AgentServiceSupervisor.h
    src/agent/RagIndexCoordinator.cpp
    src/agent/RagIndexCoordinator.h
    src/sync/SyncCoordinator.cpp
    src/sync/SyncCoordinator.h
    src/PetStateEngine.cpp
    src/PetStateEngine.h
    src/logic/LogicRoleManager.cpp
    src/logic/LogicRoleManager.h
    src/logic/LogicRole.h
    src/logic/StateAnalysisRole.cpp
    src/logic/StateAnalysisRole.h
    src/logic/HealthCareRole.cpp
    src/logic/HealthCareRole.h
    src/logic/DreamGenerationRole.cpp
    src/logic/DreamGenerationRole.h
    src/logic/DiaryStickerRole.cpp
    src/logic/DiaryStickerRole.h
    src/logic/CognitiveRoutingRole.cpp
    src/logic/CognitiveRoutingRole.h
    src/AiCredentialStore.cpp
    src/AiCredentialStore.h
    src/AiService.cpp
    src/AiService.h
    src/VisionService.cpp
    src/VisionService.h
    src/modules/FeatureModule.h
    src/data/DataRepository.h
    src/modules/DiaryEntryModel.cpp
    src/modules/DiaryEntryModel.h
    src/modules/ModuleManager.cpp
    src/modules/ModuleManager.h
    src/modules/ReverseDiaryModule.cpp
    src/modules/ReverseDiaryModule.h
    src/modules/LongTermMemoryModule.cpp
    src/modules/LongTermMemoryModule.h
    src/modules/MemoryListModel.cpp
    src/modules/MemoryListModel.h
    src/modules/ProactiveBehaviorModule.cpp
    src/modules/ProactiveBehaviorModule.h
    src/modules/MemeCultureModule.cpp
    src/modules/MemeCultureModule.h
    src/modules/AdaptiveLearningModule.cpp
    src/modules/AdaptiveLearningModule.h
    src/modules/FileSnackModule.cpp
    src/modules/FileSnackModule.h
    src/modules/DataCleanupModule.cpp
    src/modules/DataCleanupModule.h
    src/modules/SummaryMagicModule.cpp
    src/modules/SummaryMagicModule.h
    src/modules/DreamModule.cpp
    src/modules/DreamModule.h
    src/modules/VisionRecognitionModule.cpp
    src/modules/VisionRecognitionModule.h
    src/modules/MorningLollipopModule.cpp
    src/modules/MorningLollipopModule.h
)
add_custom_target(CopyAgentRuntime ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EmotionSprite>/agent-runtime"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/agent-core/dist/agent-core"
            "$<TARGET_FILE_DIR:EmotionSprite>/agent-runtime"
    DEPENDS EmotionSprite)
if(MSVC)
    target_link_options(EmotionSprite PRIVATE "/MAP:${CMAKE_BINARY_DIR}/EmotionSprite.map")
endif()

qt_add_qml_module(EmotionSprite
    URI EmotionSprite
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ChatWindow.qml
        qml/SettingsWindow.qml
        qml/DiaryWindow.qml
        qml/MemoryWindow.qml
        qml/ProactiveWindow.qml
        qml/LearningWindow.qml
        qml/DebugWindow.qml
        qml/MouseChaser.qml
        qml/FileSnackWindow.qml
        qml/DataCleanupWindow.qml
        qml/SummaryMagicWindow.qml
        qml/DreamBottleWindow.qml
        qml/MorningLollipopWindow.qml
)

qt_add_resources(EmotionSprite "sprite_assets"
    PREFIX "/"
    FILES
        assets/states/stellacandie_00_attentive_clean.png
        assets/states/stellacandie_00_neutral_clean.png
        assets/states/stellacandie_01_happy_clean.png
        assets/states/stellacandie_02_curious_clean.png
        assets/states/stellacandie_03_angry_clean.png
        assets/states/stellacandie_04_pouting_clean.png
        assets/states/stellacandie_05_affectionate_clean.png
        assets/states/stellacandie_06_shy_clean.png
        assets/states/stellacandie_07_sleepy_clean.png
        assets/states/stellacandie_08_scared_clean.png
        assets/states/stellacandie_09_sick_clean.png
        assets/states/stellacandie_10_recovering_clean.png
        assets/states/stellacandie_09_sick_magic_cold_v2.png
        assets/states/stellacandie_10_recovering_magic_cold_v2.png
        assets/states/stellacandie_11_summary_studying_v1.png
        assets/dreams/bottle/star_bottle_empty_v1.png
        assets/dreams/paper/star_paper_folded_v1.png
        assets/prompts/system_prompt.txt
        assets/memes/hot_memes_zh_CN.json
        assets/animations/stellacandie_walk_right.png
        assets/animations/stellacandie_run_right.png
        assets/animations/stellacandie_jump_right.png
        assets/animations/stellacandie_sleep_right.png
        assets/animations/mouse_run_right.png
        assets/animations/stellacandie_snack_processing_v1.png
        assets/animations/stellacandie_snack_eating_v1.png
        assets/animations/stellacandie_snack_eating_v2.png
)

target_link_libraries(EmotionSprite PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
)

if(WIN32)
    set_target_properties(EmotionSprite PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(EmotionSprite PRIVATE Advapi32 Shell32)
endif()

install(TARGETS EmotionSprite
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/agent-core/dist/agent-core/" DESTINATION bin/agent-runtime)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME AgentCoreContractTests
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/agent-core/.venv/Scripts/python.exe" -m pytest -q)
    set_tests_properties(AgentCoreContractTests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/agent-core")
    qt_add_executable(AgentClientTests
        tests/AgentClientTests.cpp
        src/agent/AgentClient.cpp
        src/agent/AgentClient.h
        src/agent/AgentServiceSupervisor.cpp
        src/agent/AgentServiceSupervisor.h
        src/AiCredentialStore.cpp
        src/AiCredentialStore.h
    )
    target_include_directories(AgentClientTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(AgentClientTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    if(WIN32)
        target_link_libraries(AgentClientTests PRIVATE Advapi32)
    endif()
    add_test(NAME AgentClientTests COMMAND AgentClientTests -v1 -o "${CMAKE_CURRENT_BINARY_DIR}/AgentClientTests.log,txt")
    set_tests_properties(AgentClientTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(Stage6QtEvaluationTests
        tests/Stage6QtEvaluationTests.cpp
        src/agent/AgentClient.cpp
        src/agent/AgentClient.h
        src/agent/AgentServiceSupervisor.cpp
        src/agent/AgentServiceSupervisor.h
        src/AiCredentialStore.cpp
        src/AiCredentialStore.h
    )
    target_compile_definitions(Stage6QtEvaluationTests PRIVATE STAGE6_GOLDEN_PATH="${CMAKE_CURRENT_SOURCE_DIR}/agent-core/evaluation/golden_set_v2.json")
    target_include_directories(Stage6QtEvaluationTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Stage6QtEvaluationTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    if(WIN32)
        target_link_libraries(Stage6QtEvaluationTests PRIVATE Advapi32)
    endif()
    add_test(NAME Stage6QtEvaluationTests COMMAND Stage6QtEvaluationTests -v1 -o "${CMAKE_CURRENT_BINARY_DIR}/Stage6QtEvaluationTests.log,txt")
    set_tests_properties(Stage6QtEvaluationTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(MigrateDatabase
        tools/MigrateDatabase.cpp
        src/StorageService.cpp
        src/StorageService.h
        src/data/SchemaMigrator.cpp
        src/data/SchemaMigrator.h
    )
    target_include_directories(MigrateDatabase PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(MigrateDatabase PRIVATE Qt6::Core Qt6::Sql)
    qt_add_executable(StorageRepositoryTests
        tests/StorageRepositoryTests.cpp
        src/StorageService.cpp
        src/StorageService.h
        src/data/SchemaMigrator.cpp
        src/data/SchemaMigrator.h
    )
    target_include_directories(StorageRepositoryTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(StorageRepositoryTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
    add_test(NAME StorageRepositoryTests COMMAND StorageRepositoryTests)
    set_tests_properties(StorageRepositoryTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(CognitiveRoutingTests
        tests/CognitiveRoutingTests.cpp
        src/logic/CognitiveRoutingRole.cpp
        src/logic/CognitiveRoutingRole.h
    )
    target_include_directories(CognitiveRoutingTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(CognitiveRoutingTests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME CognitiveRoutingTests COMMAND CognitiveRoutingTests)
    set_tests_properties(CognitiveRoutingTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
