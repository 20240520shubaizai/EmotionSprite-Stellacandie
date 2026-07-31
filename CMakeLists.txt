cmake_minimum_required(VERSION 3.21)

project(EmotionSprite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Network Quick Qml Sql Widgets)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(EmotionSprite
    src/main.cpp
    src/AppController.cpp
    src/AppController.h
    src/ChatMessageModel.cpp
    src/ChatMessageModel.h
    src/StorageService.cpp
    src/StorageService.h
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
    target_link_libraries(EmotionSprite PRIVATE Advapi32)
endif()

install(TARGETS EmotionSprite
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
