#include <avz.h>
#include <string>
#include <minwindef.h>
#include <unordered_set>
#include <memory>

// 常量
constexpr int COB_TIME = 373;       // 炮发射到命中的时间
constexpr int ICE_TIME = 298;       // 寒冰菇生效时间
constexpr int N_TIME = 299;         // 毁灭菇生效时间
constexpr int HIT_TIME = 341;       // 加速波炮命中时间
constexpr float COB_HIT_COL = 8.8f; // 冰波的炮命中列数
constexpr int INITIAL_SUN = 2000;   // 初始阳光值
constexpr bool WAVE_PAINTER_ENABLED = true;     // 是否启用波数绘制
constexpr bool COB_HP_PAINTER_ENABLED = true;   // 是否启用炮生命绘制
constexpr bool CONTINUOUS_MODE = false;          // 连续全难度冲关。为false则为2F演示
constexpr int N_PLANT_TIME = 1488 - 200;    // W9/W19毁灭菇的种植时间点
constexpr int COB_EMPTY_STATE = 35; // 空炮状态值

// 日志对象
ALogger<AConsole> logger;

// 节奏: ch5: PP | IPP-PP | IPP-PP
// 时机:      601| 1438   | 601
static const int wave_lengths[21] = {
    -1, 
    601, 
    1150, 
    601,
    1438, 
    601,
    1438,
    1438,
    601, 
    -1,
    601,
    1150,
    601,
    1438,
    601,
    1438,
    1438,
    601,
    1438,
    -1,
    -1
};

static const std::unordered_set<int> wave_set[] = { 
    // PP waves
    {1, 3, 5, 8, 10, 12, 14, 17}, 
    // IPP-N waves
    {2, 11}, 
    // IPP-PP waves
    {4, 6, 7, 13, 15, 16, 18} 
};

// 重置阳光
void resetSun() {
    AGetMainObject()->Sun() = INITIAL_SUN;
    logger.Info("The sun is initially set to #.", INITIAL_SUN);
}

// 种植
void plantN(int row, float column) {
    ACard({{AHMG_15, row, column}});
    ACard({{AKFD_35, row, column}});
}

// 绘制类
class Painter {
    private:
        std::unique_ptr<ATickRunner> runner;

    public: 
        ~Painter() {
            if (runner) {
                runner->Stop();
            }
        }

        void start() {
            if (!runner) {
                runner = std::make_unique<ATickRunner>();
            }
            // 启动线程运行 draw()
            runner->Start([this]() {draw();});
        }

        void stop() {
            if (runner) {
                runner->Stop();
                runner.reset();
            }
        }

    protected: 
        // 强制子类实现 draw()
        virtual void draw() = 0;
};

// 波数绘制类
class WavePainter : public Painter {
    private: 
        void draw() override {
            aPainter.SetFont("BrianneTod");
            aPainter.SetTextColor(AArgb(0xff, 255, 255, 255));
            aPainter.SetRectColor(AArgb(0x80, 0, 0, 0));
            int current_wave = getWave();
            std::string wave_text = "Wave " + std::to_string(current_wave);
            // 越界检查
            if (current_wave >= 0 && current_wave <= 20) {
                int current_wave_length = wave_lengths[current_wave];
                if (current_wave_length != -1) {
                    wave_text += ": " + std::to_string(current_wave_length);
                }
            } else {
                logger.Warning("Invalid wave number.");
            }

            aPainter.Draw(AText(wave_text, 58, 550));
        }

        // 获取当前波数
        int getWave() {
            return AGetMainObject()->Wave();
        }
};

// 炮生命绘制类
class CobHpPainter : public Painter {
    private: 
        void draw() override {
            aPainter.SetFont("BrianneTod");
            aPainter.SetTextColor(AArgb(0xff, 41, 248, 244));
            aPainter.SetRectColor(AArgb(0x0, 0, 0, 0));
            for (auto&& plant : aAlivePlantFilter) {
                if (plant.Type() == AYMJNP_47 && plant.Col() == 0) {
                    // 只绘制第一列的玉米炮（底线炮）的生命值
                    int hp = plant.Hp();
                    if (hp < 300) {
                        // 有炮损的炮用红色的字体标出HP
                        aPainter.SetTextColor(AArgb(0xff, 192, 0, 0));
                    } else {
                        // 没有炮损时的字体颜色
                        aPainter.SetTextColor(AArgb(0xff, 41, 248, 244));
                    }
                    aPainter.Draw(AText("HP:" + std::to_string(hp),
                        plant.Abscissa(), plant.Ordinate()));
                }
            }
        }
};

// 垫材管理类
class FodderManager{
    public: 
        // 可用垫材列表
        std::vector<APlantType> fodders = {APUFF_SHROOM, ASUN_SHROOM, ASCAREDY_SHROOM, AFLOWER_POT, ASQUASH};
        // 垫红眼线程
        ATickRunner block_giga;
        // 红眼僵尸列表
        AAliveFilter<AZombie> giga;

        FodderManager()
        : giga([](AZombie* zombie) { return zombie->Type() == AGIGA_GARGANTUAR; }){}

        ~FodderManager() {
            block_giga.Stop();
        }

        // 于指定位置放置垫材1次
        void plantFodder(int row, float column) {
            int index = fodderUsable();
            if (index != -1) {
                ACard({{fodders[index], row, column}});
            }
        }
    
        // 检查垫材是否可用，如果可用则返回可用的最小索引，否则返回-1
        int fodderUsable() {
            int index;
            for (index = 0; index < fodders.size(); ++index) {
                if (AIsSeedUsable(fodders[index])) {
                    return index;
                }
            }
            return -1;
        }

        // 启动垫红眼线程
        void startBlockGargantuar(int row, float column) {
            if (!block_giga.IsStopped()) return;
            block_giga.Start([this, row, column] {
                blockGargantuar(row, column);
            });
        }

        // 停止垫红眼线程
        void stopBlockGargantuar() {
            block_giga.Stop(); 
        }

    private:
        
        // 将可用的垫材种在指定的位置(row, column)，用于在线程里循环运行
        // 只负责垫在指定的位置，什么时候开始结束垫不归这里管
        void blockGargantuar(int row, float column) {
            for (auto& zombie : giga) {
                if (isGigaHammer(zombie, row - 1, column * 80 - 40)) {
                    return;
                }
            }

            int index = fodderUsable();
            if (AGetPlantIndex(row, column) == -1 && index != -1) {
                ACard({{fodders[index], row, column}});
            }
        }

        // 用于判断巨人是否正在举锤的函数
        // 借鉴自向量cwl的经典2炮挂机脚本
        float hammerRate(AZombie& zombie)
        {
            auto animationCode = zombie.MRef<uint16_t>(0x118);
            auto animationArray = AGetPvzBase()
                                    ->AnimationMain()
                                    ->AnimationOffset()
                                    ->AnimationArray();
            auto circulationRate = animationArray[animationCode].CirculationRate();
            return circulationRate - 0.644;
        }

        // 判断巨人是否正在执行“砸/举锤”这个动作（即这个时候不能放垫）
        // 借鉴自向量cwl的经典2炮挂机脚本
        bool isGigaHammer(AZombie& zombie, int plantRow, int plantX) {
            if (!zombie.IsHammering()) {
                return false;
            }
            if (zombie.Row() != plantRow) {
                return false;
            }
            if (hammerRate(zombie) > 0) {
                return false;
            }

            std::pair<int, int> zombieAtk = {zombie.Abscissa() - 30, zombie.Abscissa() - 30 + 89};
            auto plantDef = std::make_pair(30, 50);
            plantDef.first += plantX;
            plantDef.second += plantX;
            return std::max(zombieAtk.first, plantDef.first) <= std::min(zombieAtk.second, plantDef.second);
        }
};

class FinalWaveHandler {
    private:
        int wave = 0;
        int giga_count[7] = {0};
        int target_row = 1;
        ATickRunner giga_runner;
        ATickRunner cleanup_runner;
        ATickRunner pogo_runner;
        ATickRunner cobs_detect_runner;
        FodderManager fodder_manager;
        AAliveFilter<AZombie> giga;
        AAliveFilter<AZombie> pogo;
        static constexpr float SNOW_PEA_COL = 4.0f;
        static constexpr float FODDER_COL = 8.0f;
        bool row_selected = false;
        bool dancer_cheat = false;

    public:
        FinalWaveHandler() = default;

        FinalWaveHandler(int wave) 
            : wave(wave),
            target_row(1),
            giga([](AZombie* zombie) { return zombie->Type() == AGIGA_GARGANTUAR; }), 
            pogo([](AZombie* zombie) { return zombie->Type() == APOGO_ZOMBIE; }), 
            row_selected(false),
            dancer_cheat(false)
            {}

        ~FinalWaveHandler() {
            logger.Info("Wave # Handler is destroyed. ", wave);
        }

        // 线程启动
        void start() {
            // 启动线程确定收尾路
            giga_runner.Start([this] {
                if (row_selected) {
                    return;
                }

                logger.Info("The w# handler starts running.", wave);

                countGiga();

                // 在1、5路中挑选更优的收尾路
                int giga1 = giga_count[1];
                int giga5 = giga_count[5];

                if (giga1 == 0 && giga5 == 0) {
                    logger.Warning("Since in wave #, the number of Gigas in both row 1 and row 5 is zero, use maid cheat. ", wave); 
                    // 因为1、5路均无本波红眼，所以一开始就用女仆秘籍
                    // 把舞王控制在炮击范围之外拖时间
                    dancer_cheat = true;
                    AMaidCheats::Dancing();
                    row_selected = true;
                    giga_runner.Stop();
                    return;  
                } else if (giga1 > 0 && giga5 > 0) {
                    target_row = (giga1 < giga5) ? 1 : 5;  
                } else if (giga1 > 0) {
                    target_row = 1; 
                } else if (giga5 > 0) {
                    target_row = 5; 
                }

                logger.Info("The target row is # in wave #. ", target_row, wave);

                row_selected = true;
                giga_runner.Stop();
            });

            // 在此之前会固定打全3炮
            // 根据红眼的分布情况决定第4炮的位置
            int cob_start_time = (wave == 20) ? 500 : N_PLANT_TIME + N_TIME + 215 - COB_TIME + 1;
            AConnect(ATime(wave, cob_start_time), [=, this] {
                if (!row_selected) {return;}
                if (dancer_cheat) {
                    // 当`dancer_cheat`为真时，只有中3路有本波红眼
                    // 只用往中路发一炮即可
                    aCobManager.RecoverFire({{3, 9}});
                } else if (target_row == 1) {
                    aCobManager.RecoverFire({{3, 9}, {4, 9}});
                } else if (target_row == 5) {
                    aCobManager.RecoverFire({{2, 9}, {3, 9}});
                }
            });

            // 以下操作是基于用红眼拖时间的决定下完成，而不是女仆舞王拖时间
            // 20s时的时间连接
            AConnect(ATime(wave, 2000), [=, this] {
                if (row_selected && !dancer_cheat) {
                    // 在目标行种植寒冰射手
                    ACard({{AHBSS_5, target_row, SNOW_PEA_COL}});

                    // 开启垫红眼线程
                    fodder_manager.startBlockGargantuar(target_row, FODDER_COL);
                }
            });

            if (wave == 9 || wave == 19) {
                // 25s时启动女仆，这里是为了控制住伴舞
                // 因为这两波都是冰波，所以没有炸全的伴舞会导致破阵
                AConnect(ATime(wave, 2500), [=] {
                    if (row_selected && !dancer_cheat) {
                        // 启动女仆秘籍，防止伴舞前进
                        AMaidCheats::Dancing();
                    }
                });

                // 启动线程检测一路是否有漏跳跳
                // 如果有则种寒冰射手减速
                AConnect(ATime(wave, 2751), [=] {
                    if (row_selected && !dancer_cheat) {
                        pogo_runner.Start([this] {
                            for (auto& zombie : pogo) { 
                                if (zombie.Row() == 0 && zombie.IsDead() == false) {
                                    ACard({{AHBSS_5, 1, SNOW_PEA_COL}});
                                    logger.Warning("Alive pogo detected in row 1! Planting Snow Pea to slow it down.");
                                    break;
                                }
                            }
                            pogo_runner.Stop();
                            return;
                        });
                    }
                });
            }

            if (wave == 20) {
                // 5500时刻为白字
                // 种倭瓜
                AConnect(ATime(wave, 5500), [this] {
                    stop(20, 5500);

                    if (row_selected && !dancer_cheat) {
                        ACard({{ASQUASH, target_row, FODDER_COL}});
                    }
                });

                // 启动线程，如果全场的炮都恢复了，则没有必要继续拖时间
                AConnect(ATime(wave, 341 - COB_TIME + 3475), [this] {
                    cobs_detect_runner.Start([this] {
                        for (auto& plant : aAlivePlantFilter) {
                            if (plant.Type() == ACOB_CANNON && plant.State() == COB_EMPTY_STATE) {
                                return;
                            }
                        }

                        if (row_selected && !dancer_cheat) {
                            fodder_manager.stopBlockGargantuar();
                            removePlants();
                            ACard({{ASQUASH, target_row, FODDER_COL}});
                            logger.Info("All cob cannons have recovered. Planting Squash to finish the wave.");
                            cobs_detect_runner.Stop();
                        }
                    });

                });

                // 启动线程，如果场上没有红眼僵尸，则铲除多余植物
                cleanup_runner.Start([this] {  
                    if (giga.Empty() && AGetPvzBase()->GameUi() == 3) {
                        removePlants();
                        cleanup_runner.Stop();
                    }
                });
            }
        }

        // 停止，在游戏中调用
        void stop(int stop_wave, int stop_block_time) {
            // 第0秒时立刻连接的操作
            // 女仆秘籍停止、线程停止
            AMaidCheats::Stop();

            // 指定时间停止的操作
            // 停止垫红眼的线程
            AConnect(ATime(stop_wave, stop_block_time), [this] {
                fodder_manager.stopBlockGargantuar();
                removePlants();
            });

            logger.Info("Wave # Handler is stopped at wave #.", wave, stop_wave);
        }

        // 退出游戏的状态钩时调用
        void exit() {
            fodder_manager.stopBlockGargantuar();
            AMaidCheats::Stop();
            stopRunners();

            // 重置状态变量
            row_selected = false; 
            dancer_cheat = false;
            
            logger.Warning("Wave # Handler terminated following game exit.", wave);
        }

        // 停止所有线程
        void stopRunners() {
            giga_runner.Stop();
            cleanup_runner.Stop();
            pogo_runner.Stop();
            cobs_detect_runner.Stop();
        }

        // 铲除寒冰射手和垫材
        void removePlants() {
            ARemovePlant(1, SNOW_PEA_COL);
            ARemovePlant(5, SNOW_PEA_COL);
            if (row_selected && !dancer_cheat) {
                ARemovePlant(target_row, FODDER_COL);
            }
        }
        
        // 获取当前波下指定行的红眼数量
        int getCurrentWaveGigaNum(int row) {
            return giga_count[row];
        }

    private:
        // 红眼计数
        void countGiga() {
            for (int i = 0; i < 7; ++i) {
                giga_count[i] = 0;
            }

            for (auto& zombie : giga) {
                if (zombie.AtWave() + 1 == wave) {
                    int row = zombie.Row() + 1;
                    giga_count[row] += 1;
                } 
            }
        }
};

// 波数绘制类对象
WavePainter wave_painter;

// 炮生命绘制类对象
CobHpPainter cob_hp_painter;

// 特殊波次处理类对象
FinalWaveHandler w9_handler(9);
FinalWaveHandler w19_handler(19);
FinalWaveHandler w20_handler(20);

// 最初加载状态钩
AOnBeforeScript({
    logger.Info("|----------------------------|");
    logger.Info("The script is loaded. ");

    if (AGetPvzBase()->GameUi() == 2) {
        // 在选卡界面设置阳光
        resetSun();
    }
});

// 进入游戏状态钩
AOnEnterFight({
    logger.Info("Enter the fight. ");

    // 启动绘制线程
    if (WAVE_PAINTER_ENABLED) {
        wave_painter.start();
    }
    if (COB_HP_PAINTER_ENABLED) {
        cob_hp_painter.start();
    }

    // 调整用炮使用顺序
    aCobManager.SetList({
        {1, 1},
        {4, 1}, 
        {2, 1}, 
        {5, 1},
        {1, 6},
        {4, 6}, 
        {2, 6}, 
        {5, 6}, 
        {3, 1}, 
        {3, 6}}
    );

    // 存冰位置
    aIceFiller.Start({
        {3, 4},
        {2, 4}, 
        {4, 4}, 
        {1, 4}, 
        {5, 4}
    });
});

void AScript()
{
    if (CONTINUOUS_MODE) {
        ASetReloadMode(AReloadMode::MAIN_UI_OR_FIGHT_UI);
        logger.Info("Current mode: Continuous Survival.");
    } else {
        ASetReloadMode(AReloadMode::MAIN_UI);
        logger.Info("Current mode: 2-Flag Survival.");
    }

    // zombies
    ASetZombies({
        APOLE_VAULTING_ZOMBIE, 
        ADANCING_ZOMBIE, 
        AZOMBONI, 
        AJACK_IN_THE_BOX_ZOMBIE, 
        ADIGGER_ZOMBIE, 
        APOGO_ZOMBIE, 
        ABUNGEE_ZOMBIE, 
        ALADDER_ZOMBIE, 
        ACATAPULT_ZOMBIE, 
        AGARGANTUAR, 
        AGIGA_GARGANTUAR, 
        ABALLOON_ZOMBIE
    });

    // plants
    ASelectCards({
        AICE_SHROOM, 
        AM_ICE_SHROOM, 
        ACOFFEE_BEAN, 
        ADOOM_SHROOM, 
        ASNOW_PEA, 
        ASQUASH, 
        APUFF_SHROOM, 
        ASUN_SHROOM, 
        ASCAREDY_SHROOM, 
        AFLOWER_POT
    });

    // 假定波长
    for (int wave = 1; wave <= 18; ++wave) {
        if (wave != 9) {
            AAssumeWavelength({ATime(wave, wave_lengths[wave])});
        }
    }

    // 操作绑定
    for (auto wave = 1; wave <= 20; ++wave) {
        // PP waves
        if (wave_set[0].contains(wave)) {
            AConnect(ATime(wave, HIT_TIME - COB_TIME), [=] {
                aCobManager.Fire({{2, 9}, {4, 9}});
            });
        } 

        // IPP-N waves
        if (wave_set[1].contains(wave)) {
            // Ice
            AConnect(ATime(wave, 1 - ICE_TIME), [=] {
                aIceFiller.Coffee();
            });

            // 热过渡PP
            AConnect(ATime(wave, 200 - COB_TIME), [=] {
                aCobManager.Fire({{2, 8.5}, {4, 8.5}});
            });

            // N
            AConnect(ATime(wave, wave_lengths[wave] - 200 - N_TIME), [=]{
                int row = 2;
                float col = (wave == 2) ? 9.0f : 8.0f;
                plantN(row, col);
            });
        }

        // IPP-PP waves
        if (wave_set[2].contains(wave)) {
            // Ice
            AConnect(ATime(wave, 1 - ICE_TIME), [=] {
                aIceFiller.Coffee();
            });

            // 热过渡PP
            AConnect(ATime(wave, 200 - COB_TIME), [=] {
                aCobManager.Fire({{2, 8.5}, {4, 8.5}});
            });

            // 激活PP
            AConnect(ATime(wave, wave_lengths[wave] - 200 - COB_TIME), [=]{
                aCobManager.Fire({{2, COB_HIT_COL}, {4, COB_HIT_COL}});
            });
        }
        
        // Wave 9 or Wave 19
        if (wave == 9 || wave == 19) {
            // 启动自动收尾线程
            AConnect(ATime(wave, 0), [=] {
                if (wave == 9) {
                    w9_handler.start();
                } else {
                    w19_handler.start();
                }
            });

            // Ice
            AConnect(ATime(wave, 1 - ICE_TIME), [=] {
                aIceFiller.Coffee();
            });

            // 热过渡PP
            AConnect(ATime(wave, 200 - COB_TIME), [=] {
                aCobManager.Fire({{2, 8.5}, {4, 8.5}});
            });

            // PP
            AConnect(ATime(wave, 1438 - 200 - COB_TIME), [=] {
                aCobManager.Fire({{2, COB_HIT_COL}, {4, COB_HIT_COL}});
            });

            // N
            AConnect(ATime(wave, N_PLANT_TIME), [=] {
                int row = 3;
                float col = (wave == 9) ? 9.0f : 8.0f;
                plantN(row, col);
            });

            // N造成伤害后的减速拦截
            AConnect(ATime(wave, N_PLANT_TIME + N_TIME + 215 - COB_TIME), [=] {
                aCobManager.Fire({{1, 8.8}, {4, 8.8}});
            });
        }

        // Wave 10
        if (wave == 10 || wave == 20) {
            AConnect(ATime(wave, 0), [=] {
                if (wave == 10) {
                    w9_handler.stop(wave, HIT_TIME);
                } else {
                    w19_handler.stop(wave, HIT_TIME);
                }
            });
        }

        // Wave 20
        if (wave == 20) {
            AConnect(ATime(wave, HIT_TIME - COB_TIME), [=] {
                aCobManager.Fire({{2, 9}, {4, 9}});
            });

            AConnect(ATime(wave, HIT_TIME + 100 - COB_TIME), [=] {
                aCobManager.RecoverFire({{2, 9}, {4, 9}});
            });

            // 调整炮炸时间以炸全舞伴
            AConnect(ATime(wave, 1800 - 200 - COB_TIME), [=] {
                aCobManager.RecoverFire({{2, 9}, {4, 9}});
            });

            AConnect(ATime(wave, 0), [=] {
                w20_handler.start();
            });

            // 最后一次用冰完成后，停止进一步存冰，维持阵型
            AConnect(ATime(wave, HIT_TIME + 100 - ICE_TIME), [=] {
                aIceFiller.Coffee();
                aIceFiller.Stop();
                aIceFiller.Start({
                    {3, 4}
                });
            });
        }
    }    
}



// 退出战斗状态钩
AOnExitFight({
    wave_painter.stop();
    cob_hp_painter.stop();
    w9_handler.exit();
    w19_handler.exit();
    w20_handler.exit();
    logger.Info("The game is closed. ");
    logger.Info("|----------------------------|");    
});

// 异常退出
AOnBeforeExit({
    wave_painter.stop();
    cob_hp_painter.stop();
    w9_handler.exit();
    w19_handler.exit();
    w20_handler.exit();
    logger.Warning("The game interrupted due to an exception. ");
});

