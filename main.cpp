#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif

#include <QWidget>
#include <QApplication>
#include <QDebug>
#include <QGamepadManager>
#include <QGamepad>
#include <QtEndian>
#include <QUdpSocket>
#include <QTimer>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QDialog>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QSettings>
#include <QComboBox>
#include <QPainter>
#include <QKeyEvent>
#include <QLabel>
#include <QTableWidget>
#include <QHBoxLayout>
#include <QHeaderView>

#include <algorithm>
#include <cmath>
#include <functional>

#define CPAD_BOUND          0x5d0
#define CPP_BOUND           0x7f

#define TOUCH_SCREEN_WIDTH  320
#define TOUCH_SCREEN_HEIGHT 240

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

double lx = 0.0, ly = 0.0;
double rx = 0.0, ry = 0.0;
QGamepadManager::GamepadButtons buttons;
u32 interfaceButtons = 0;

// Multi-IP support: Replace single ipAddress with a list
struct IPTarget {
    QString address;
    bool enabled;
    QString name;  // Optional display name
};
QList<IPTarget> ipTargets;
int yAxisMultiplier = 1;
bool abInverse = false;
bool xyInverse = false;

bool touchScreenPressed;
QPoint touchScreenPosition;

QSettings settings("TuxSH", "InputRedirectionClient-Qt");

// Turbo VC Reset button variables
QTimer *turboVcResetTimer = nullptr;
QTimer *turboATimer = nullptr; // Global reference to turbo A timer
bool turboVcResetActive = false;
int turboVcResetStage = 0;
int turboVcResetCount = 0;
const int TURBO_VC_RESET_STAGES = 3; // Number of stages in the sequence
int turboVcResetIntervalMs = settings.value("turboVcResetIntervalMs", 500).toInt(); // 0.5 seconds between VC resets
int turboVcResetWaitMs = settings.value("turboVcResetWaitMs", 5000).toInt(); // 5 seconds wait before turbo A
int turboADurationMs = settings.value("turboADurationMs", 13500).toInt(); // 13.5 seconds
const int TURBO_A_INTERVAL_MS = 250;   // 0.25 seconds
int turboAMaxCount = turboADurationMs / TURBO_A_INTERVAL_MS; // Calculate max count based on duration

// Turbo Moon Reset button variables
QTimer *turboMoonResetTimer = nullptr;
QTimer *turboMoonATimer = nullptr;
bool turboMoonResetActive = false;
int turboMoonResetStage = 0;
int turboMoonResetCount = 0;

int turboMoonResetWaitMs = settings.value("turboMoonResetWaitMs", 5000).toInt(); // 5 seconds wait before turbo A
int turboMoonADurationMs = settings.value("turboMoonADurationMs", 25000).toInt(); // 25 seconds
int turboMoonAIntervalMs = settings.value("turboMoonAIntervalMs", 1000).toInt();   // 1 second
int turboMoonAMaxCount = turboMoonADurationMs / turboMoonAIntervalMs; // Calculate max count based on duration

// Direction hold timers and settings for Moon Reset
QTimer *moonDirHoldStartTimer = nullptr;
QTimer *moonDirHoldStopTimer = nullptr;
int moonResetDirectionDelayMs = settings.value("moonResetDirectionDelayMs", 0).toInt();
int moonResetDirectionDurationMs = settings.value("moonResetDirectionDurationMs", turboMoonADurationMs).toInt();

// Moon reset joystick direction
enum MoonResetDirection {
    MOON_RESET_NONE = 0,
    MOON_RESET_UP = 1,
    MOON_RESET_DOWN = 2,
    MOON_RESET_LEFT = 3,
    MOON_RESET_RIGHT = 4,
    MOON_RESET_UP_LEFT = 5,
    MOON_RESET_UP_RIGHT = 6,
    MOON_RESET_DOWN_LEFT = 7,
    MOON_RESET_DOWN_RIGHT = 8
};
int moonResetDirection = settings.value("moonResetDirection", MOON_RESET_NONE).toInt();

// Reset counter variables
int resetCounter = settings.value("resetCounter", 0).toInt();

QGamepadManager::GamepadButton variantToButton(QVariant variant)
{
    QGamepadManager::GamepadButton button;

    button = static_cast<QGamepadManager::GamepadButton>(variant.toInt());

    return button;
}

QGamepadManager::GamepadButton homeButton = variantToButton(settings.value("ButtonHome", QGamepadManager::ButtonInvalid));
QGamepadManager::GamepadButton powerButton = variantToButton(settings.value("ButtonPower", QGamepadManager::ButtonInvalid));
QGamepadManager::GamepadButton powerLongButton = variantToButton(settings.value("ButtonPowerLong", QGamepadManager::ButtonInvalid));

QGamepadManager::GamepadButton touchButton1 = variantToButton(settings.value("ButtonT1", QGamepadManager::ButtonInvalid));
QGamepadManager::GamepadButton touchButton2 = variantToButton(settings.value("ButtonT2", QGamepadManager::ButtonInvalid));
int touchButton1X = settings.value("touchButton1X", 0).toInt(),
    touchButton1Y = settings.value("touchButton1Y", 0).toInt(),
    touchButton2X = settings.value("touchButton2X", 0).toInt(),
    touchButton2Y = settings.value("touchButton2Y", 0).toInt();

QGamepadManager::GamepadButton hidButtonsAB[] = {
    variantToButton(settings.value("ButtonA", QGamepadManager::ButtonA)),
    variantToButton(settings.value("ButtonB", QGamepadManager::ButtonB)),
};

QGamepadManager::GamepadButton hidButtonsMiddle[] = {
    variantToButton(settings.value("ButtonSelect", QGamepadManager::ButtonSelect)),
    variantToButton(settings.value("ButtonStart", QGamepadManager::ButtonStart)),
    variantToButton(settings.value("ButtonRight", QGamepadManager::ButtonRight)),
    variantToButton(settings.value("ButtonLeft", QGamepadManager::ButtonLeft)),
    variantToButton(settings.value("ButtonUp", QGamepadManager::ButtonUp)),
    variantToButton(settings.value("ButtonDown", QGamepadManager::ButtonDown)),
    variantToButton(settings.value("ButtonR", QGamepadManager::ButtonR1)),
    variantToButton(settings.value("ButtonL", QGamepadManager::ButtonL1)),
};

QGamepadManager::GamepadButton hidButtonsXY[] = {
    variantToButton(settings.value("ButtonX", QGamepadManager::ButtonX)),
    variantToButton(settings.value("ButtonY", QGamepadManager::ButtonY)),
};

QGamepadManager::GamepadButton irButtons[] = {
    variantToButton(settings.value("ButtonZR", QGamepadManager::ButtonR2)),
    variantToButton(settings.value("ButtonZL", QGamepadManager::ButtonL2)),
};

/*QGamepadManager::GamepadButton speButtons[] = {
    QGamepadManager::ButtonL3,
    QGamepadManager::ButtonR3,
    QGamepadManager::ButtonGuide,
};*/

void sendFrame(void)
{
    u32 hidPad = 0xfff;
    if(!abInverse)
    {
        for(u32 i = 0; i < 2; i++)
        {
            if(buttons & (1 << hidButtonsAB[i]))
                hidPad &= ~(1 << i);
        }
    }
    else
    {
        for(u32 i = 0; i < 2; i++)
        {
            if(buttons & (1 << hidButtonsAB[1-i]))
                hidPad &= ~(1 << i);
        }
    }

    for(u32 i = 2; i < 10; i++)
    {
        if(buttons & (1 << hidButtonsMiddle[i-2]))
            hidPad &= ~(1 << i);
    }

    if(!xyInverse)
    {
        for(u32 i = 10; i < 12; i++)
        {
            if(buttons & (1 << hidButtonsXY[i-10]))
                hidPad &= ~(1 << i);
        }
    }
    else
    {
        for(u32 i = 10; i < 12; i++)
        {
            if(buttons & (1 << hidButtonsXY[1-(i-10)]))
                hidPad &= ~(1 << i);
        }
    }

    u32 irButtonsState = 0;
    for(u32 i = 0; i < 2; i++)
    {
            if(buttons & (1 << irButtons[i]))
                irButtonsState |= 1 << (i + 1);
    }

    /*u32 specialButtonsState = 0;
    for(u32 i = 0; i < 3; i++)
    {

        if(buttons & (1 << speButtons[i]))
            specialButtonsState |= 1 << i;
    }
    specialButtonsState |= interfaceButtons;*/

    u32 touchScreenState = 0x2000000;
    u32 circlePadState = 0x7ff7ff;
    u32 cppState = 0x80800081;

    if(lx != 0.0 || ly != 0.0)
    {
        u32 x = (u32)(lx * CPAD_BOUND + 0x800);
        u32 y = (u32)(ly * CPAD_BOUND + 0x800);
        x = x >= 0xfff ? (lx < 0.0 ? 0x000 : 0xfff) : x;
        y = y >= 0xfff ? (ly < 0.0 ? 0x000 : 0xfff) : y;

        circlePadState = (y << 12) | x;
    }

    if(rx != 0.0 || ry != 0.0 || irButtonsState != 0)
    {
        // We have to rotate the c-stick position 45°. Thanks, Nintendo.
        u32 x = (u32)(M_SQRT1_2 * (rx + ry) * CPP_BOUND + 0x80);
        u32 y = (u32)(M_SQRT1_2 * (ry - rx) * CPP_BOUND + 0x80);
        x = x >= 0xff ? (rx < 0.0 ? 0x00 : 0xff) : x;
        y = y >= 0xff ? (ry < 0.0 ? 0x00 : 0xff) : y;

        cppState = (y << 24) | (x << 16) | (irButtonsState << 8) | 0x81;
    }

    if(touchScreenPressed)
    {
        u32 x = (u32)(0xfff * std::min(std::max(0, touchScreenPosition.x()), TOUCH_SCREEN_WIDTH)) / TOUCH_SCREEN_WIDTH;
        u32 y = (u32)(0xfff * std::min(std::max(0, touchScreenPosition.y()), TOUCH_SCREEN_HEIGHT)) / TOUCH_SCREEN_HEIGHT;
        touchScreenState = (1 << 24) | (y << 12) | x;
    }

    QByteArray ba(20, 0);
    qToLittleEndian(hidPad, (uchar *)ba.data());
    qToLittleEndian(touchScreenState, (uchar *)ba.data() + 4);
    qToLittleEndian(circlePadState, (uchar *)ba.data() + 8);
    qToLittleEndian(cppState, (uchar *)ba.data() + 12);
    qToLittleEndian(interfaceButtons, (uchar *)ba.data() + 16);
    for (const auto& target : ipTargets) {
        if (target.enabled) {
            QUdpSocket().writeDatagram(ba, QHostAddress(target.address), 4950);
        }
    }
}

void startTurboVcReset()
{
    if (turboVcResetActive) return;
    
    qDebug() << "Starting Turbo VC Reset sequence...";
    turboVcResetActive = true;
    turboVcResetStage = 0;
    turboVcResetCount = 0;
    
    if (!turboVcResetTimer) {
        turboVcResetTimer = new QTimer();
        turboVcResetTimer->setSingleShot(true);
        QObject::connect(turboVcResetTimer, &QTimer::timeout, []() {
            if (!turboVcResetActive) return;
            
            switch (turboVcResetStage) {
                case 0: // Stage 1: Trigger VC Reset 3 times
                    if (turboVcResetCount < TURBO_VC_RESET_STAGES) {
                        qDebug() << "VC Reset" << (turboVcResetCount + 1) << "of" << TURBO_VC_RESET_STAGES;
                        // Trigger VC Reset
                        touchScreenPressed = true;
                        touchScreenPosition = QPoint(TOUCH_SCREEN_WIDTH - 75, TOUCH_SCREEN_HEIGHT - 55);
                        sendFrame();
                        
                        // Release after 50ms
                        QTimer::singleShot(50, [=]() {
                            touchScreenPressed = false;
                            sendFrame();
                        });
                        
                        turboVcResetCount++;
                        
                        // Schedule next VC Reset
                        if (turboVcResetCount < TURBO_VC_RESET_STAGES) {
                            turboVcResetTimer->start(turboVcResetIntervalMs);
                        } else {
                            // Move to stage 2: Wait 5 seconds
                            qDebug() << "VC Reset sequence complete. Waiting" << (turboVcResetWaitMs/1000.0) << "seconds before Turbo A...";
                            turboVcResetStage = 1;
                            turboVcResetTimer->start(turboVcResetWaitMs);
                        }
                    }
                    break;
                    
                case 1: // Stage 2: Start Turbo A sequence
                    qDebug() << "Starting Turbo A sequence...";
                    turboVcResetStage = 2;
                    turboVcResetCount = 0;
                    
                    // Start the turbo A timer
                    turboATimer = new QTimer();
                    turboATimer->setSingleShot(false);
                    QObject::connect(turboATimer, &QTimer::timeout, []() {
                        if (turboVcResetActive && turboVcResetCount < turboAMaxCount) {
                            // Press A button
                            buttons |= QGamepadManager::GamepadButtons(1 << hidButtonsAB[0]);
                            sendFrame();
                            
                            // Release A button after a short delay
                            QTimer::singleShot(50, [=]() {
                                buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsAB[0]));
                                sendFrame();
                            });
                            
                            turboVcResetCount++;
                        } else {
                            // Stop turbo mode
                            qDebug() << "Turbo VC Reset sequence complete!";
                            turboVcResetActive = false;
                            turboATimer->stop();
                            turboATimer->deleteLater();
                            turboATimer = nullptr;
                        }
                    });
                    
                    turboATimer->start(TURBO_A_INTERVAL_MS);
                    break;
            }
        });
    }
    
    // Start the sequence
    turboVcResetTimer->start(0); // Start immediately
}

void stopTurboVcReset()
{
    if (!turboVcResetActive) return;
    
    qDebug() << "Stopping Turbo VC Reset sequence...";
    turboVcResetActive = false;
    
    if (turboVcResetTimer) {
        turboVcResetTimer->stop();
    }
    
    if (turboATimer) {
        turboATimer->stop();
        turboATimer->deleteLater();
        turboATimer = nullptr;
    }
    
    // Reset any active button states
    buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsAB[0]));
    touchScreenPressed = false;
    sendFrame();
}

void startTurboMoonReset()
{
    if (turboMoonResetActive) return;
    
    qDebug() << "Starting Turbo Moon Reset sequence...";
    turboMoonResetActive = true;
    turboMoonResetStage = 0;
    turboMoonResetCount = 0;
    
    if (!turboMoonResetTimer) {
        turboMoonResetTimer = new QTimer();
        turboMoonResetTimer->setSingleShot(true);
        QObject::connect(turboMoonResetTimer, &QTimer::timeout, []() {
            if (!turboMoonResetActive) return;
            
            switch (turboMoonResetStage) {
                case 0: // Stage 1: Press L+R+select+start
                    qDebug() << "Pressing L+R+select+start for Moon Reset";
                    // Press L+R+select+start
                    buttons |= QGamepadManager::GamepadButtons(1 << hidButtonsMiddle[6]); // R
                    buttons |= QGamepadManager::GamepadButtons(1 << hidButtonsMiddle[7]); // L
                    buttons |= QGamepadManager::GamepadButtons(1 << hidButtonsMiddle[0]); // Select
                    buttons |= QGamepadManager::GamepadButtons(1 << hidButtonsMiddle[1]); // Start
                    sendFrame();
                    
                    // Release after 100ms
                    QTimer::singleShot(100, [=]() {
                        buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsMiddle[6])); // R
                        buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsMiddle[7])); // L
                        buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsMiddle[0])); // Select
                        buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsMiddle[1])); // Start
                        sendFrame();
                    });
                    
                    // Move to stage 2: Wait 5 seconds
                    qDebug() << "Moon Reset complete. Waiting" << (turboMoonResetWaitMs/1000.0) << "seconds before Turbo A...";
                    turboMoonResetStage = 1;
                    turboMoonResetTimer->start(turboMoonResetWaitMs);
                    break;
                    
                case 1: // Stage 2: Start Turbo A sequence
                    qDebug() << "Starting Turbo A sequence for Moon Reset...";
                    turboMoonResetStage = 2;
                    turboMoonResetCount = 0;
                    
                    // Schedule joystick direction hold if selected
                    if (moonResetDirection != MOON_RESET_NONE) {
                        if (!moonDirHoldStartTimer) { moonDirHoldStartTimer = new QTimer(); moonDirHoldStartTimer->setSingleShot(true); }
                        if (!moonDirHoldStopTimer)  { moonDirHoldStopTimer  = new QTimer(); moonDirHoldStopTimer->setSingleShot(true); }

                        QObject::connect(moonDirHoldStartTimer, &QTimer::timeout, [](){
                            lx = 0.0; ly = 0.0;
                            switch (moonResetDirection) {
                                case MOON_RESET_UP:          ly = -1.0; break;
                                case MOON_RESET_DOWN:        ly =  1.0; break;
                                case MOON_RESET_LEFT:        lx = -1.0; break;
                                case MOON_RESET_RIGHT:       lx =  1.0; break;
                                case MOON_RESET_UP_LEFT:     lx = -M_SQRT1_2; ly = -M_SQRT1_2; break;
                                case MOON_RESET_UP_RIGHT:    lx =  M_SQRT1_2; ly = -M_SQRT1_2; break;
                                case MOON_RESET_DOWN_LEFT:   lx = -M_SQRT1_2; ly =  M_SQRT1_2; break;
                                case MOON_RESET_DOWN_RIGHT:  lx =  M_SQRT1_2; ly =  M_SQRT1_2; break;
                                default: break;
                            }
                            sendFrame();
                        });

                        QObject::connect(moonDirHoldStopTimer, &QTimer::timeout, [](){
                            lx = 0.0; ly = 0.0; sendFrame();
                        });

                        moonDirHoldStartTimer->start(moonResetDirectionDelayMs);
                        moonDirHoldStopTimer->start(moonResetDirectionDelayMs + moonResetDirectionDurationMs);
                        qDebug() << "Scheduled direction hold with delay" << moonResetDirectionDelayMs << "and duration" << moonResetDirectionDurationMs;
                    }
                    
                    // Start the turbo A timer
                    turboMoonATimer = new QTimer();
                    turboMoonATimer->setSingleShot(false);
                    QObject::connect(turboMoonATimer, &QTimer::timeout, []() {
                        if (turboMoonResetActive && turboMoonResetCount < turboMoonAMaxCount) {
                            // Press A button
                            buttons |= QGamepadManager::GamepadButtons(1 << hidButtonsAB[0]);
                            sendFrame();
                            
                            // Release A button after a short delay
                            QTimer::singleShot(50, [=]() {
                                buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsAB[0]));
                                sendFrame();
                            });
                            
                            turboMoonResetCount++;
                        } else {
                            // Stop turbo mode and reset joystick
                            qDebug() << "Turbo Moon Reset sequence complete!";
                            turboMoonResetActive = false;
                            turboMoonATimer->stop();
                            turboMoonATimer->deleteLater();
                            turboMoonATimer = nullptr;
                            
                            // Reset joystick position
                            lx = 0.0;
                            ly = 0.0;
                            sendFrame();
                        }
                    });
                    
                    turboMoonATimer->start(turboMoonAIntervalMs);
                    break;
            }
        });
    }
    
    // Start the sequence
    turboMoonResetTimer->start(0); // Start immediately
}

void stopTurboMoonReset()
{
    if (!turboMoonResetActive) return;
    
    qDebug() << "Stopping Turbo Moon Reset sequence...";
    turboMoonResetActive = false;
    
    if (turboMoonResetTimer) {
        turboMoonResetTimer->stop();
    }
    
    if (turboMoonATimer) {
        turboMoonATimer->stop();
        turboMoonATimer->deleteLater();
        turboMoonATimer = nullptr;
    }
    if (moonDirHoldStartTimer) {
        moonDirHoldStartTimer->stop();
        moonDirHoldStartTimer->deleteLater();
        moonDirHoldStartTimer = nullptr;
    }
    if (moonDirHoldStopTimer) {
        moonDirHoldStopTimer->stop();
        moonDirHoldStopTimer->deleteLater();
        moonDirHoldStopTimer = nullptr;
    }
    
    // Reset any active button states
    buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsAB[0]));
    buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsMiddle[6])); // R
    buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsMiddle[7])); // L
    buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsMiddle[0])); // Select
    buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsMiddle[1])); // Start
    
    // Reset joystick position
    lx = 0.0;
    ly = 0.0;
    sendFrame();
}

void stopAllProcesses()
{
    stopTurboVcReset();
    stopTurboMoonReset();
}

void incrementResetCounter()
{
    resetCounter++;
    settings.setValue("resetCounter", resetCounter);
}

void resetCounterToZero()
{
    resetCounter = 0;
    settings.setValue("resetCounter", resetCounter);
}

struct GamepadMonitor : public QObject {

    GamepadMonitor(QObject *parent = nullptr) : QObject(parent)
    {
        connect(QGamepadManager::instance(), &QGamepadManager::gamepadButtonPressEvent, this,
            [](int deviceId, QGamepadManager::GamepadButton button, double value)
        {
            (void)deviceId;
            (void)value;
            buttons |= QGamepadManager::GamepadButtons(1 << button);

            if (button == homeButton)
            {
                interfaceButtons |= 1;
            }
            if (button == powerButton)
            {
                interfaceButtons |= 2;
            }
            if (button == powerLongButton)
            {
                interfaceButtons |= 4;
            }

            if (button == touchButton1)
            {
                touchScreenPressed = true;
                touchScreenPosition = QPoint(touchButton1X, touchButton1Y);
            }
            if (button == touchButton2)
            {
                touchScreenPressed = true;
                touchScreenPosition = QPoint(touchButton2X, touchButton2Y);
            }


            sendFrame();
        });

        connect(QGamepadManager::instance(), &QGamepadManager::gamepadButtonReleaseEvent, this,
            [](int deviceId, QGamepadManager::GamepadButton button)
        {
            (void)deviceId;
            buttons &= QGamepadManager::GamepadButtons(~(1 << button));

            if (button == homeButton)
            {
                interfaceButtons &= ~1;
            }
            if (button == powerButton)
            {
                interfaceButtons &= ~2;
            }
            if (button == powerLongButton)
            {
                interfaceButtons &= ~4;
            }

            if ((button == touchButton1) || (button == touchButton2))
            {
                touchScreenPressed = false;
            }

            sendFrame();
        });
        connect(QGamepadManager::instance(), &QGamepadManager::gamepadAxisEvent, this,
            [](int deviceId, QGamepadManager::GamepadAxis axis, double value)
        {
            (void)deviceId;
            (void)value;
            switch(axis)
            {
                case QGamepadManager::AxisLeftX:
                    lx = value;
                    break;
                case QGamepadManager::AxisLeftY:
                    ly = yAxisMultiplier * -value; // for some reason qt inverts this
                    break;

                case QGamepadManager::AxisRightX:
                    rx = value;
                    break;
                case QGamepadManager::AxisRightY:
                    ry = yAxisMultiplier * -value; // for some reason qt inverts this
                    break;
                default: break;
            }
            sendFrame();
        });
    }
};

struct TouchScreen : public QDialog {
    TouchScreen(QWidget *parent = nullptr) : QDialog(parent)
    {
        this->setFixedSize(TOUCH_SCREEN_WIDTH, TOUCH_SCREEN_HEIGHT);
        this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        this->setWindowTitle(tr("InputRedirectionClient-Qt - Touch screen"));
    }

    void mousePressEvent(QMouseEvent *ev)
    {
        if(ev->button() == Qt::LeftButton)
        {
            touchScreenPressed = true;
            touchScreenPosition = ev->pos();
            sendFrame();
        }
    }

    void mouseMoveEvent(QMouseEvent *ev)
    {
        if(touchScreenPressed && (ev->buttons() & Qt::LeftButton))
        {
            touchScreenPosition = ev->pos();
            sendFrame();
        }
    }

    void mouseReleaseEvent(QMouseEvent *ev)
    {
        if(ev->button() == Qt::LeftButton)
        {
            touchScreenPressed = false;
            sendFrame();
        }
    }

    void closeEvent(QCloseEvent *ev)
    {
        touchScreenPressed = false;
        sendFrame();
        ev->accept();
    }

    void paintEvent(QPaintEvent *)
    {
        QPainter painter(this);

        if (settings.value("ButtonT1", QGamepadManager::ButtonInvalid) != QGamepadManager::ButtonInvalid)
        {
            QPen pen(QColor("#f00"));
            painter.setPen(pen);
            painter.drawEllipse(QPoint(settings.value("touchButton1X", 0).toInt(), settings.value("touchButton1Y", 0).toInt()), 3, 3);
        }
        if (settings.value("ButtonT2", QGamepadManager::ButtonInvalid) != QGamepadManager::ButtonInvalid)
        {
            QPen pen(QColor("#00f"));
            painter.setPen(pen);
            painter.drawEllipse(QPoint(settings.value("touchButton2X", 0).toInt(), settings.value("touchButton2Y", 0).toInt()), 3, 3);
        }
    }
};

struct FrameTimer : public QTimer {
    FrameTimer(QObject *parent = nullptr) : QTimer(parent)
    {
        connect(this, &QTimer::timeout, this,
                [](void)
        {
            sendFrame();
        });
    }
};

struct RemapConfig : public QDialog {
private:
    QVBoxLayout *layout;
    QFormLayout *formLayout;
    QComboBox *comboBoxA, *comboBoxB, *comboBoxX, *comboBoxY, *comboBoxL, *comboBoxR,
        *comboBoxUp, *comboBoxDown, *comboBoxLeft, *comboBoxRight, *comboBoxStart, *comboBoxSelect,
        *comboBoxZL, *comboBoxZR, *comboBoxHome, *comboBoxPower, *comboBoxPowerLong, *comboBoxTouch1, *comboBoxTouch2;
    QLineEdit *touchButton1XEdit, *touchButton1YEdit,
        *touchButton2XEdit, *touchButton2YEdit;
    QPushButton *saveButton, *closeButton;

    QComboBox* populateItems(QGamepadManager::GamepadButton button)
    {
        QComboBox *comboBox = new QComboBox();
        comboBox->addItem("A (bottom)", QGamepadManager::ButtonA);
        comboBox->addItem("B (right)", QGamepadManager::ButtonB);
        comboBox->addItem("X (left)", QGamepadManager::ButtonX);
        comboBox->addItem("Y (top)", QGamepadManager::ButtonY);
        comboBox->addItem("Right", QGamepadManager::ButtonRight);
        comboBox->addItem("Left", QGamepadManager::ButtonLeft);
        comboBox->addItem("Up", QGamepadManager::ButtonUp);
        comboBox->addItem("Down", QGamepadManager::ButtonDown);
        comboBox->addItem("RB", QGamepadManager::ButtonR1);
        comboBox->addItem("LB", QGamepadManager::ButtonL1);
        comboBox->addItem("Select", QGamepadManager::ButtonSelect);
        comboBox->addItem("Start", QGamepadManager::ButtonStart);
        comboBox->addItem("RT", QGamepadManager::ButtonR2);
        comboBox->addItem("LT", QGamepadManager::ButtonL2);
        comboBox->addItem("L3", QGamepadManager::ButtonL3);
        comboBox->addItem("R3", QGamepadManager::ButtonR3);
        comboBox->addItem("Guide", QGamepadManager::ButtonGuide);
        comboBox->addItem("None", QGamepadManager::ButtonInvalid);

        int index = comboBox->findData(button);
        comboBox->setCurrentIndex(index);

        return comboBox;
    }

    QVariant currentData(QComboBox *comboBox)
    {
        QVariant variant;

        variant = comboBox->itemData(comboBox->currentIndex());

        return variant;
    }

public:
    RemapConfig(QWidget *parent = nullptr, QDialog *ts = nullptr) : QDialog(parent)
    {
        this->setFixedSize(TOUCH_SCREEN_WIDTH, 700);
        this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        this->setWindowTitle(tr("InputRedirectionClient-Qt - Button Config"));

        layout = new QVBoxLayout(this);

        comboBoxA = populateItems(variantToButton(settings.value("ButtonA", QGamepadManager::ButtonA)));
        comboBoxB = populateItems(variantToButton(settings.value("ButtonB", QGamepadManager::ButtonB)));
        comboBoxX = populateItems(variantToButton(settings.value("ButtonX", QGamepadManager::ButtonX)));
        comboBoxY = populateItems(variantToButton(settings.value("ButtonY", QGamepadManager::ButtonY)));
        comboBoxUp = populateItems(variantToButton(settings.value("ButtonUp", QGamepadManager::ButtonUp)));
        comboBoxDown = populateItems(variantToButton(settings.value("ButtonDown", QGamepadManager::ButtonDown)));
        comboBoxLeft = populateItems(variantToButton(settings.value("ButtonLeft", QGamepadManager::ButtonLeft)));
        comboBoxRight = populateItems(variantToButton(settings.value("ButtonRight", QGamepadManager::ButtonRight)));
        comboBoxL = populateItems(variantToButton(settings.value("ButtonL", QGamepadManager::ButtonL1)));
        comboBoxR = populateItems(variantToButton(settings.value("ButtonR", QGamepadManager::ButtonR1)));
        comboBoxSelect = populateItems(variantToButton(settings.value("ButtonSelect", QGamepadManager::ButtonSelect)));
        comboBoxStart = populateItems(variantToButton(settings.value("ButtonStart", QGamepadManager::ButtonStart)));
        comboBoxZL = populateItems(variantToButton(settings.value("ButtonZL", QGamepadManager::ButtonL2)));
        comboBoxZR = populateItems(variantToButton(settings.value("ButtonZR", QGamepadManager::ButtonR2)));
        comboBoxHome = populateItems(variantToButton(settings.value("ButtonHome", QGamepadManager::ButtonInvalid)));
        comboBoxPower = populateItems(variantToButton(settings.value("ButtonPower", QGamepadManager::ButtonInvalid)));
        comboBoxPowerLong = populateItems(variantToButton(settings.value("ButtonPowerLong", QGamepadManager::ButtonInvalid)));
        comboBoxTouch1 = populateItems(variantToButton(settings.value("ButtonT1", QGamepadManager::ButtonInvalid)));
        comboBoxTouch2 = populateItems(variantToButton(settings.value("ButtonT2", QGamepadManager::ButtonInvalid)));

        touchButton1XEdit = new QLineEdit(this);
        touchButton1XEdit->setClearButtonEnabled(true);
        touchButton1XEdit->setText(settings.value("touchButton1X", "0").toString());
        touchButton1YEdit = new QLineEdit(this);
        touchButton1YEdit->setClearButtonEnabled(true);
        touchButton1YEdit->setText(settings.value("touchButton1Y", "0").toString());

        touchButton2XEdit = new QLineEdit(this);
        touchButton2XEdit->setClearButtonEnabled(true);
        touchButton2XEdit->setText(settings.value("touchButton2X", "0").toString());
        touchButton2YEdit = new QLineEdit(this);
        touchButton2YEdit->setClearButtonEnabled(true);
        touchButton2YEdit->setText(settings.value("touchButton2Y", "0").toString());

        formLayout = new QFormLayout;

        formLayout->addRow(tr("A Button"), comboBoxA);
        formLayout->addRow(tr("B Button"), comboBoxB);
        formLayout->addRow(tr("X Button"), comboBoxX);
        formLayout->addRow(tr("Y Button"), comboBoxY);
        formLayout->addRow(tr("DPad-Up"), comboBoxUp);
        formLayout->addRow(tr("DPad-Down"), comboBoxDown);
        formLayout->addRow(tr("DPad-Left"), comboBoxLeft);
        formLayout->addRow(tr("DPad-Right"), comboBoxRight);
        formLayout->addRow(tr("L Button"), comboBoxL);
        formLayout->addRow(tr("R Button"), comboBoxR);
        formLayout->addRow(tr("Select"), comboBoxSelect);
        formLayout->addRow(tr("Start"), comboBoxStart);
        formLayout->addRow(tr("Home"), comboBoxHome);
        formLayout->addRow(tr("Power"), comboBoxPower);
        formLayout->addRow(tr("Power-Long"), comboBoxPowerLong);
        formLayout->addRow(tr("ZL Button"), comboBoxZL);
        formLayout->addRow(tr("ZR Button"), comboBoxZR);
        formLayout->addRow(tr("Touch Button 1"), comboBoxTouch1);
        formLayout->addRow(tr("Touch Button X"), touchButton1XEdit);
        formLayout->addRow(tr("Touch Button Y"), touchButton1YEdit);
        formLayout->addRow(tr("Touch Button 2"), comboBoxTouch2);
        formLayout->addRow(tr("Touch Button X"), touchButton2XEdit);
        formLayout->addRow(tr("Touch Button Y"), touchButton2YEdit);

        saveButton = new QPushButton(tr("&SAVE"), this);
        closeButton = new QPushButton(tr("&CANCEL"), this);

        layout->addLayout(formLayout);
        layout->addWidget(saveButton);
        layout->addWidget(closeButton);

        connect(touchButton1XEdit, &QLineEdit::textChanged, this,
                [ts](const QString &text)
        {
            touchButton1X = text.toUInt();
            ts->update();
            settings.setValue("touchButton1X", text);
        });
        connect(touchButton1YEdit, &QLineEdit::textChanged, this,
                [ts](const QString &text)
        {
            touchButton1Y = text.toUInt();
            ts->update();
            settings.setValue("touchButton1Y", text);
        });
        connect(touchButton2XEdit, &QLineEdit::textChanged, this,
                [ts](const QString &text)
        {
            touchButton2X = text.toUInt();
            ts->update();
            settings.setValue("touchButton2X", text);
        });
        connect(touchButton2YEdit, &QLineEdit::textChanged, this,
                [ts](const QString &text)
        {
            touchButton2Y = text.toUInt();
            ts->update();
            settings.setValue("touchButton2Y", text);
        });

        connect(saveButton, &QPushButton::pressed, this,
                [this, ts](void)
        {
            QGamepadManager::GamepadButton a = variantToButton(currentData(comboBoxA));
            hidButtonsAB[0] = a;
            settings.setValue("ButtonA", a);
            QGamepadManager::GamepadButton b = variantToButton(currentData(comboBoxB));
            hidButtonsAB[1] = b;
            settings.setValue("ButtonB", b);

            QGamepadManager::GamepadButton select = variantToButton(currentData(comboBoxSelect));
            hidButtonsMiddle[0] = select;
            settings.setValue("ButtonSelect", select);
            QGamepadManager::GamepadButton start = variantToButton(currentData(comboBoxStart));
            hidButtonsMiddle[1] = start;
            settings.setValue("ButtonStart", start);
            QGamepadManager::GamepadButton right = variantToButton(currentData(comboBoxRight));
            hidButtonsMiddle[2] = right;
            settings.setValue("ButtonRight", right);
            QGamepadManager::GamepadButton left = variantToButton(currentData(comboBoxLeft));
            hidButtonsMiddle[3] = left;
            settings.setValue("ButtonLeft", left);
            QGamepadManager::GamepadButton up = variantToButton(currentData(comboBoxUp));
            hidButtonsMiddle[4] = up;
            settings.setValue("ButtonUp", up);
            QGamepadManager::GamepadButton down = variantToButton(currentData(comboBoxDown));
            hidButtonsMiddle[5] = down;
            settings.setValue("ButtonDown", down);
            QGamepadManager::GamepadButton r = variantToButton(currentData(comboBoxR));
            hidButtonsMiddle[6] = r;
            settings.setValue("ButtonR", r);
            QGamepadManager::GamepadButton l = variantToButton(currentData(comboBoxL));
            hidButtonsMiddle[7] = l;
            settings.setValue("ButtonL", l);

            QGamepadManager::GamepadButton x = variantToButton(currentData(comboBoxX));
            hidButtonsXY[0] = x;
            settings.setValue("ButtonX", x);
            QGamepadManager::GamepadButton y = variantToButton(currentData(comboBoxY));
            hidButtonsXY[1] = y;
            settings.setValue("ButtonY", y);

            QGamepadManager::GamepadButton zr = variantToButton(currentData(comboBoxZR));
            irButtons[0] = zr;
            settings.setValue("ButtonZR", zr);
            QGamepadManager::GamepadButton zl = variantToButton(currentData(comboBoxZL));
            irButtons[1] = zl;
            settings.setValue("ButtonZL", zl);

            QGamepadManager::GamepadButton power = variantToButton(currentData(comboBoxPower));
            powerButton = power;
            settings.setValue("ButtonPower", power);
            QGamepadManager::GamepadButton powerLong = variantToButton(currentData(comboBoxPowerLong));
            powerLongButton = powerLong;
            settings.setValue("ButtonPowerLong", powerLong);
            QGamepadManager::GamepadButton home = variantToButton(currentData(comboBoxHome));
            homeButton = home;
            settings.setValue("ButtonHome", home);

            QGamepadManager::GamepadButton t1 = variantToButton(currentData(comboBoxTouch1));
            touchButton1 = t1;
            settings.setValue("ButtonT1", t1);
            QGamepadManager::GamepadButton t2 = variantToButton(currentData(comboBoxTouch2));
            touchButton2 = t2;
            settings.setValue("ButtonT2", t2);
            ts->update();

        });
        connect(closeButton, &QPushButton::pressed, this,
                [this](void)
        {
           this->hide();
        });
    }
};

struct IPManagerDialog : public QDialog {
private:
    QVBoxLayout *layout;
    QTableWidget *ipTable;
    QPushButton *addButton, *removeButton, *saveButton, *closeButton;
    std::function<void()> statusUpdateCallback;
    
    void updateTable() {
        ipTable->setRowCount(ipTargets.size());
        for (int i = 0; i < ipTargets.size(); ++i) {
            const auto& target = ipTargets.at(i);
            
            // Enable checkbox
            QCheckBox *enabledCheck = new QCheckBox();
            enabledCheck->setChecked(target.enabled);
            ipTable->setCellWidget(i, 0, enabledCheck);
            
            // Name field
            QLineEdit *nameEdit = new QLineEdit(target.name);
            ipTable->setCellWidget(i, 1, nameEdit);
            
            // IP address field
            QLineEdit *addrEdit = new QLineEdit(target.address);
            ipTable->setCellWidget(i, 2, addrEdit);
        }
    }
    
    void saveToSettings() {
        QList<QVariant> addresses, enabledStates, names;
        
        for (int i = 0; i < ipTable->rowCount(); ++i) {
            QCheckBox *enabledCheck = qobject_cast<QCheckBox*>(ipTable->cellWidget(i, 0));
            QLineEdit *nameEdit = qobject_cast<QLineEdit*>(ipTable->cellWidget(i, 1));
            QLineEdit *addrEdit = qobject_cast<QLineEdit*>(ipTable->cellWidget(i, 2));
            
            if (enabledCheck && nameEdit && addrEdit) {
                addresses.append(addrEdit->text());
                enabledStates.append(enabledCheck->isChecked());
                names.append(nameEdit->text());
            }
        }
        
        settings.setValue("ipAddresses", addresses);
        settings.setValue("ipEnabled", enabledStates);
        settings.setValue("ipNames", names);
        
        // Update global ipTargets
        ipTargets.clear();
        for (int i = 0; i < addresses.size(); ++i) {
            IPTarget target;
            target.address = addresses.at(i).toString();
            target.enabled = enabledStates.at(i).toBool();
            target.name = names.at(i).toString();
            ipTargets.append(target);
        }
        
        // Call status update callback if provided
        if (statusUpdateCallback) {
            statusUpdateCallback();
        }
    }

public:
    IPManagerDialog(QWidget *parent = nullptr, std::function<void()> callback = nullptr) : QDialog(parent), statusUpdateCallback(callback) {
        this->setFixedSize(500, 400);
        this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        this->setWindowTitle(tr("InputRedirectionClient-Qt - IP Manager"));

        layout = new QVBoxLayout(this);
        
        // Create table
        ipTable = new QTableWidget(this);
        ipTable->setColumnCount(3);
        ipTable->setHorizontalHeaderLabels({tr("Enabled"), tr("Name"), tr("IP Address")});
        ipTable->horizontalHeader()->setStretchLastSection(true);
        
        // Buttons
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        addButton = new QPushButton(tr("&Add IP"), this);
        removeButton = new QPushButton(tr("&Remove"), this);
        QPushButton *enableAllButton = new QPushButton(tr("Enable &All"), this);
        QPushButton *disableAllButton = new QPushButton(tr("Disable &All"), this);
        saveButton = new QPushButton(tr("&Save"), this);
        closeButton = new QPushButton(tr("&Cancel"), this);
        
        buttonLayout->addWidget(addButton);
        buttonLayout->addWidget(removeButton);
        buttonLayout->addWidget(enableAllButton);
        buttonLayout->addWidget(disableAllButton);
        buttonLayout->addStretch();
        buttonLayout->addWidget(saveButton);
        buttonLayout->addWidget(closeButton);
        
        layout->addWidget(ipTable);
        layout->addLayout(buttonLayout);
        
        // Connect signals
        connect(addButton, &QPushButton::clicked, this, [this]() {
            ipTable->insertRow(ipTable->rowCount());
            int row = ipTable->rowCount() - 1;
            
            QCheckBox *enabledCheck = new QCheckBox();
            enabledCheck->setChecked(true);
            ipTable->setCellWidget(row, 0, enabledCheck);
            
            QLineEdit *nameEdit = new QLineEdit(tr("Target %1").arg(row + 1));
            ipTable->setCellWidget(row, 1, nameEdit);
            
            QLineEdit *addrEdit = new QLineEdit();
            addrEdit->setPlaceholderText("192.168.1.100");
            ipTable->setCellWidget(row, 2, addrEdit);
        });
        
        connect(removeButton, &QPushButton::clicked, this, [this]() {
            int currentRow = ipTable->currentRow();
            if (currentRow >= 0) {
                ipTable->removeRow(currentRow);
            }
        });
        
        connect(enableAllButton, &QPushButton::clicked, this, [this]() {
            for (int i = 0; i < ipTable->rowCount(); ++i) {
                if (QCheckBox *enabledCheck = qobject_cast<QCheckBox*>(ipTable->cellWidget(i, 0))) {
                    enabledCheck->setChecked(true);
                }
            }
        });
        
        connect(disableAllButton, &QPushButton::clicked, this, [this]() {
            for (int i = 0; i < ipTable->rowCount(); ++i) {
                if (QCheckBox *enabledCheck = qobject_cast<QCheckBox*>(ipTable->cellWidget(i, 0))) {
                    enabledCheck->setChecked(false);
                }
            }
        });
        
        connect(saveButton, &QPushButton::clicked, this, [this]() {
            saveToSettings();
            this->accept();
        });
        
        connect(closeButton, &QPushButton::clicked, this, [this]() {
            this->reject();
        });
        
        // Initialize table with current data
        updateTable();
    }
};

class Widget : public QWidget
{
private:
    QVBoxLayout *layout;
    QFormLayout *formLayout;
    QCheckBox *invertYCheckbox, *invertABCheckbox, *invertXYCheckbox;
    QPushButton *homeButton, *powerButton, *longPowerButton, *aButton, *vcResetButton, *remapConfigButton, *turboAButton, *turboMoonResetButton, *stopProcessButton, *ipManagerButton, *resetCounterButton;
    QLabel *statusLabel, *resetCounterLabel;
    TouchScreen *touchScreen;
    RemapConfig *remapConfig;
    IPManagerDialog *ipManager;
    
    // Timer configuration inputs
    QLineEdit *turboVcResetIntervalEdit, *turboVcResetWaitEdit, *turboADurationEdit;
    QLineEdit *turboMoonResetWaitEdit, *turboMoonAIntervalEdit, *turboMoonADurationEdit;
    QLineEdit *moonDirDelayEdit, *moonDirDurationEdit;
    QComboBox *moonResetDirectionCombo;
public:
    Widget(QWidget *parent = nullptr) : QWidget(parent)
    {
        layout = new QVBoxLayout(this);

        invertYCheckbox = new QCheckBox(this);
        invertABCheckbox = new QCheckBox(this);
        invertXYCheckbox = new QCheckBox(this);
        formLayout = new QFormLayout;

        // Reset counter display and button at the top
        QHBoxLayout *counterLayout = new QHBoxLayout();
        resetCounterLabel = new QLabel(tr("Reset Count: %1").arg(resetCounter), this);
        resetCounterLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; color: #4CAF50; }");
        resetCounterButton = new QPushButton(tr("Reset Counter"), this);
        resetCounterButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; padding: 5px; border-radius: 3px; } QPushButton:hover { background-color: #d32f2f; }");
        counterLayout->addWidget(resetCounterLabel);
        counterLayout->addStretch();
        counterLayout->addWidget(resetCounterButton);
        
        formLayout->addRow(tr(""), counterLayout);
        
        // Create horizontal layout for the two columns
        QHBoxLayout *timerLayout = new QHBoxLayout();
        timerLayout->setSpacing(20);
        
        // Left column - VC Reset settings
        QFormLayout *vcForm = new QFormLayout();
        vcForm->setHorizontalSpacing(10);
        vcForm->setVerticalSpacing(5);
        vcForm->addRow(tr("VC Reset (ms):"), new QLabel(tr(""), this));
        vcForm->addRow(tr("Interval:"), turboVcResetIntervalEdit = new QLineEdit(this));
        vcForm->addRow(tr("Wait:"), turboVcResetWaitEdit = new QLineEdit(this));
        vcForm->addRow(tr("Duration:"), turboADurationEdit = new QLineEdit(this));
        
        // Right column - Moon Reset settings
        QFormLayout *moonForm = new QFormLayout();
        moonForm->setHorizontalSpacing(10);
        moonForm->setVerticalSpacing(5);
        moonForm->addRow(tr("Moon Reset (ms):"), new QLabel(tr(""), this));
        moonForm->addRow(tr("Wait:"), turboMoonResetWaitEdit = new QLineEdit(this));
        moonForm->addRow(tr("Interval:"), turboMoonAIntervalEdit = new QLineEdit(this));
        moonForm->addRow(tr("Duration:"), turboMoonADurationEdit = new QLineEdit(this));
        moonForm->addRow(tr("Dir Delay (ms):"), moonDirDelayEdit = new QLineEdit(this));
        moonForm->addRow(tr("Dir Duration (ms):"), moonDirDurationEdit = new QLineEdit(this));
        
        // Add joystick direction dropdown
        moonResetDirectionCombo = new QComboBox(this);
        moonResetDirectionCombo->addItem(tr("None"), MOON_RESET_NONE);
        moonResetDirectionCombo->addItem(tr("Up"), MOON_RESET_UP);
        moonResetDirectionCombo->addItem(tr("Down"), MOON_RESET_DOWN);
        moonResetDirectionCombo->addItem(tr("Left"), MOON_RESET_LEFT);
        moonResetDirectionCombo->addItem(tr("Right"), MOON_RESET_RIGHT);
        moonResetDirectionCombo->addItem(tr("Up-Left"), MOON_RESET_UP_LEFT);
        moonResetDirectionCombo->addItem(tr("Up-Right"), MOON_RESET_UP_RIGHT);
        moonResetDirectionCombo->addItem(tr("Down-Left"), MOON_RESET_DOWN_LEFT);
        moonResetDirectionCombo->addItem(tr("Down-Right"), MOON_RESET_DOWN_RIGHT);
        moonResetDirectionCombo->setCurrentIndex(moonResetDirection);
        moonResetDirectionCombo->setToolTip(tr("Joystick direction to hold during Moon Reset duration"));
        moonForm->addRow(tr("Direction:"), moonResetDirectionCombo);
        
        timerLayout->addLayout(vcForm);
        timerLayout->addLayout(moonForm);
        
        formLayout->addRow(tr(""), timerLayout);
        
        remapConfigButton = new QPushButton(tr("BUTTON &CONFIG"), this);
        remapConfigButton->setFocusPolicy(Qt::StrongFocus);
        ipManagerButton = new QPushButton(tr("IP &MANAGER"), this);
        ipManagerButton->setFocusPolicy(Qt::StrongFocus);
        ipManagerButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px; border-radius: 4px; } QPushButton:hover { background-color: #45a049; }");

        homeButton = new QPushButton(tr("&HOME"), this);
        homeButton->setFocusPolicy(Qt::StrongFocus);
        powerButton = new QPushButton(tr("&POWER"), this);
        powerButton->setFocusPolicy(Qt::StrongFocus);
        longPowerButton = new QPushButton(tr("POWER (&long)"), this);
        longPowerButton->setFocusPolicy(Qt::StrongFocus);
        aButton = new QPushButton(tr("&A BUTTON"), this);
        aButton->setFocusPolicy(Qt::StrongFocus);
        vcResetButton = new QPushButton(tr("VC &RESET"), this);
        vcResetButton->setFocusPolicy(Qt::StrongFocus);
        turboAButton = new QPushButton(tr("TURBO VC &RESET"), this);
        turboAButton->setFocusPolicy(Qt::StrongFocus);
        turboAButton->setToolTip(tr("Start the Turbo VC Reset sequence (VC Reset x3, wait, then Turbo A)"));
        turboMoonResetButton = new QPushButton(tr("TURBO MOON &RESET"), this);
        turboMoonResetButton->setFocusPolicy(Qt::StrongFocus);
        turboMoonResetButton->setToolTip(tr("Start the Turbo Moon Reset sequence (L+R+select+start, wait 5s, then A every second for 25s)"));
        stopProcessButton = new QPushButton(tr("STOP &PROCESS"), this);
        stopProcessButton->setFocusPolicy(Qt::StrongFocus);
        stopProcessButton->setToolTip(tr("Stop any currently running turbo process (VC Reset or Moon Reset)"));

        // Initialize timer configuration inputs
        turboVcResetIntervalEdit->setClearButtonEnabled(true);
        turboVcResetIntervalEdit->setText(QString::number(turboVcResetIntervalMs));
        turboVcResetIntervalEdit->setPlaceholderText("500");
        turboVcResetIntervalEdit->setToolTip(tr("Time between VC Reset button presses (in milliseconds)"));
        
        turboVcResetWaitEdit->setClearButtonEnabled(true);
        turboVcResetWaitEdit->setText(QString::number(turboVcResetWaitMs));
        turboVcResetWaitEdit->setPlaceholderText("5000");
        turboVcResetWaitEdit->setToolTip(tr("Wait time after VC Reset sequence before starting Turbo A (in milliseconds)"));
        
        turboADurationEdit->setClearButtonEnabled(true);
        turboADurationEdit->setText(QString::number(turboADurationMs));
        turboADurationEdit->setPlaceholderText("13500");
        turboADurationEdit->setToolTip(tr("Total duration of the Turbo A sequence (in milliseconds)"));
        
        turboMoonResetWaitEdit->setClearButtonEnabled(true);
        turboMoonResetWaitEdit->setText(QString::number(turboMoonResetWaitMs));
        turboMoonResetWaitEdit->setPlaceholderText("5000");
        turboMoonResetWaitEdit->setToolTip(tr("Wait time after Moon Reset before starting Turbo A (in milliseconds)"));
        
        turboMoonAIntervalEdit->setClearButtonEnabled(true);
        turboMoonAIntervalEdit->setText(QString::number(turboMoonAIntervalMs));
        turboMoonAIntervalEdit->setPlaceholderText("1000");
        turboMoonAIntervalEdit->setToolTip(tr("Time between A button presses in Moon Reset sequence (in milliseconds)"));
        
        turboMoonADurationEdit->setClearButtonEnabled(true);
        turboMoonADurationEdit->setText(QString::number(turboMoonADurationMs));
        turboMoonADurationEdit->setPlaceholderText("25000");
        turboMoonADurationEdit->setToolTip(tr("Total duration of the Turbo A sequence for Moon Reset (in milliseconds)"));

        moonDirDelayEdit->setClearButtonEnabled(true);
        moonDirDelayEdit->setText(QString::number(moonResetDirectionDelayMs));
        moonDirDelayEdit->setPlaceholderText("0");
        moonDirDelayEdit->setToolTip(tr("Delay before holding the selected direction (in milliseconds)"));

        moonDirDurationEdit->setClearButtonEnabled(true);
        moonDirDurationEdit->setText(QString::number(moonResetDirectionDurationMs));
        moonDirDurationEdit->setPlaceholderText(QString::number(turboMoonADurationMs));
        moonDirDurationEdit->setToolTip(tr("How long to hold the selected direction (in milliseconds)"));

        layout->addLayout(formLayout);
        layout->addWidget(ipManagerButton);
        layout->addWidget(homeButton);
        layout->addWidget(powerButton);
        layout->addWidget(longPowerButton);
        layout->addWidget(aButton);
        layout->addWidget(vcResetButton);
        layout->addWidget(turboAButton);
        layout->addWidget(turboMoonResetButton);
        layout->addWidget(stopProcessButton);
        layout->addWidget(remapConfigButton);
        
        // Status label to show active IPs
        statusLabel = new QLabel(this);
        statusLabel->setWordWrap(true);
        statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        layout->addWidget(statusLabel);



        connect(invertYCheckbox, &QCheckBox::stateChanged, this,
                [](int state)
        {
            switch(state)
            {
                case Qt::Unchecked:
                    yAxisMultiplier = 1;
                    settings.setValue("invertY", false);
                    break;
                case Qt::Checked:
                    yAxisMultiplier = -1;
                    settings.setValue("invertY", true);
                    break;
                default: break;
            }
        });

        connect(invertABCheckbox, &QCheckBox::stateChanged, this,
                [](int state)
        {
            switch(state)
            {
                case Qt::Unchecked:
                    abInverse = false;
                    settings.setValue("invertAB", false);
                    break;
                case Qt::Checked:
                    abInverse = true;
                    settings.setValue("invertAB", true);
                    break;
                default: break;
            }
        });

        connect(invertXYCheckbox, &QCheckBox::stateChanged, this,
                [](int state)
        {
            switch(state)
            {
                case Qt::Unchecked:
                    xyInverse = false;
                    settings.setValue("invertXY", false);
                    break;
                case Qt::Checked:
                    xyInverse = true;
                    settings.setValue("invertXY", true);
                    break;
                default: break;
            }
        });

        // Timer configuration connections
        connect(turboVcResetIntervalEdit, &QLineEdit::textChanged, this,
                [](const QString &text)
        {
            bool ok;
            int value = text.toInt(&ok);
            if (ok && value > 0) {
                turboVcResetIntervalMs = value;
                settings.setValue("turboVcResetIntervalMs", value);
            }
        });

        connect(turboVcResetWaitEdit, &QLineEdit::textChanged, this,
                [](const QString &text)
        {
            bool ok;
            int value = text.toInt(&ok);
            if (ok && value >= 0) {
                turboVcResetWaitMs = value;
                settings.setValue("turboVcResetWaitMs", value);
            }
        });

        connect(turboADurationEdit, &QLineEdit::textChanged, this,
                [](const QString &text)
        {
            bool ok;
            int value = text.toInt(&ok);
            if (ok && value > 0) {
                turboADurationMs = value;
                turboAMaxCount = value / TURBO_A_INTERVAL_MS;
                settings.setValue("turboADurationMs", value);
            }
        });

        connect(turboMoonResetWaitEdit, &QLineEdit::textChanged, this,
                [](const QString &text)
        {
            bool ok;
            int value = text.toInt(&ok);
            if (ok && value >= 0) {
                turboMoonResetWaitMs = value;
                settings.setValue("turboMoonResetWaitMs", value);
            }
        });

        connect(turboMoonAIntervalEdit, &QLineEdit::textChanged, this,
                [](const QString &text)
        {
            bool ok;
            int value = text.toInt(&ok);
            if (ok && value > 0) {
                turboMoonAIntervalMs = value;
                turboMoonAMaxCount = turboMoonADurationMs / value;
                settings.setValue("turboMoonAIntervalMs", value);
            }
        });

        connect(turboMoonADurationEdit, &QLineEdit::textChanged, this,
                [](const QString &text)
        {
            bool ok;
            int value = text.toInt(&ok);
            if (ok && value > 0) {
                turboMoonADurationMs = value;
                turboMoonAMaxCount = value / turboMoonAIntervalMs;
                settings.setValue("turboMoonADurationMs", value);
            }
        });

        connect(moonResetDirectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [](int index)
        {
            moonResetDirection = index;
            settings.setValue("moonResetDirection", index);
        });

        connect(moonDirDelayEdit, &QLineEdit::textChanged, this,
                [](const QString &text)
        {
            bool ok; int value = text.toInt(&ok);
            if (ok && value >= 0) {
                moonResetDirectionDelayMs = value;
                settings.setValue("moonResetDirectionDelayMs", value);
            }
        });

        connect(moonDirDurationEdit, &QLineEdit::textChanged, this,
                [](const QString &text)
        {
            bool ok; int value = text.toInt(&ok);
            if (ok && value >= 0) {
                moonResetDirectionDurationMs = value;
                settings.setValue("moonResetDirectionDurationMs", value);
            }
        });

        connect(homeButton, &QPushButton::pressed, this,
                [](void)
        {
           interfaceButtons |= 1;
           sendFrame();
        });

        connect(homeButton, &QPushButton::released, this,
                [](void)
        {
           interfaceButtons &= ~1;
           sendFrame();
        });

        connect(powerButton, &QPushButton::pressed, this,
                [](void)
        {
           interfaceButtons |= 2;
           sendFrame();
        });

        connect(powerButton, &QPushButton::released, this,
                [](void)
        {
           interfaceButtons &= ~2;
           sendFrame();
        });

        connect(longPowerButton, &QPushButton::pressed, this,
                [](void)
        {
           interfaceButtons |= 4;
           sendFrame();
        });

        connect(longPowerButton, &QPushButton::released, this,
                [](void)
        {
           interfaceButtons &= ~4;
           sendFrame();
        });

        connect(aButton, &QPushButton::pressed, this,
                [](void)
        {
           buttons |= QGamepadManager::GamepadButtons(1 << hidButtonsAB[0]);
           sendFrame();
        });

        connect(aButton, &QPushButton::released, this,
                [](void)
        {
           buttons &= QGamepadManager::GamepadButtons(~(1 << hidButtonsAB[0]));
           sendFrame();
        });

        connect(vcResetButton, &QPushButton::pressed, this,
                [this](void)
        {
           touchScreenPressed = true;
           touchScreenPosition = QPoint(TOUCH_SCREEN_WIDTH - 75, TOUCH_SCREEN_HEIGHT - 55);
           sendFrame();
           incrementResetCounter();
           resetCounterLabel->setText(tr("Reset Count: %1").arg(resetCounter));
        });

        connect(vcResetButton, &QPushButton::released, this,
                [](void)
        {
           touchScreenPressed = false;
           sendFrame();
        });

        connect(turboAButton, &QPushButton::pressed, this,
                [this](void)
        {
           startTurboVcReset();
           incrementResetCounter();
           resetCounterLabel->setText(tr("Reset Count: %1").arg(resetCounter));
        });

        connect(turboMoonResetButton, &QPushButton::pressed, this,
                [this](void)
        {
           startTurboMoonReset();
           incrementResetCounter();
           resetCounterLabel->setText(tr("Reset Count: %1").arg(resetCounter));
        });

        connect(stopProcessButton, &QPushButton::pressed, this,
                [](void)
        {
           stopAllProcesses();
        });

        connect(resetCounterButton, &QPushButton::pressed, this,
                [this](void)
        {
           resetCounterToZero();
           resetCounterLabel->setText(tr("Reset Count: %1").arg(resetCounter));
        });

        connect(remapConfigButton, &QPushButton::released, this,
                [this](void)
        {
           remapConfig->show();
        });

        connect(ipManagerButton, &QPushButton::released, this,
                [this](void)
        {
           ipManager->show();
        });

        touchScreen = new TouchScreen(nullptr);
        remapConfig = new RemapConfig(nullptr, touchScreen);
        ipManager = new IPManagerDialog(nullptr, [this]() { updateStatusDisplay(); });
        this->setWindowTitle(tr("InputRedirectionClient-Qt"));

        // Initialize ipTargets from settings
        QList<QVariant> ipAddresses = settings.value("ipAddresses", QVariantList()).toList();
        QList<QVariant> enabledStates = settings.value("ipEnabled", QVariantList()).toList();
        QList<QVariant> names = settings.value("ipNames", QVariantList()).toList();

        for (int i = 0; i < ipAddresses.size(); ++i) {
            IPTarget target;
            target.address = ipAddresses.at(i).toString();
            target.enabled = enabledStates.at(i).toBool();
            target.name = names.at(i).toString();
            ipTargets.append(target);
        }

        // If no IPs are configured, add a default one for backward compatibility
        if (ipTargets.isEmpty()) {
            QString defaultIP = settings.value("ipAddress", "").toString();
            if (!defaultIP.isEmpty()) {
                IPTarget target;
                target.address = defaultIP;
                target.enabled = true;
                target.name = tr("Default");
                ipTargets.append(target);
            }
        }


        
        // Update status display
        updateStatusDisplay();
        
        invertYCheckbox->setChecked(settings.value("invertY", false).toBool());
        invertABCheckbox->setChecked(settings.value("invertAB", false).toBool());
        invertXYCheckbox->setChecked(settings.value("invertXY", false).toBool());
    }

    void show(void)
    {
        QWidget::show();
        touchScreen->show();
        remapConfig->hide();
    }

    void closeEvent(QCloseEvent *ev) override
    {
        touchScreen->close();
        remapConfig->close();
        ipManager->close();
        ev->accept();
    }

    virtual ~Widget(void)
    {
        lx = ly = rx = ry = 0.0;
        buttons = QGamepadManager::GamepadButtons();
        interfaceButtons = 0;
        touchScreenPressed = false;
        
        // Stop and cleanup turbo timers
        if (turboVcResetTimer) {
            turboVcResetTimer->stop();
            delete turboVcResetTimer;
            turboVcResetTimer = nullptr;
        }
        if (turboATimer) {
            turboATimer->stop();
            delete turboATimer;
            turboATimer = nullptr;
        }
        if (turboMoonResetTimer) {
            turboMoonResetTimer->stop();
            delete turboMoonResetTimer;
            turboMoonResetTimer = nullptr;
        }
        if (turboMoonATimer) {
            turboMoonATimer->stop();
            delete turboMoonATimer;
            turboMoonATimer = nullptr;
        }
        if (moonDirHoldStartTimer) {
            moonDirHoldStartTimer->stop();
            delete moonDirHoldStartTimer;
            moonDirHoldStartTimer = nullptr;
        }
        if (moonDirHoldStopTimer) {
            moonDirHoldStopTimer->stop();
            delete moonDirHoldStopTimer;
            moonDirHoldStopTimer = nullptr;
        }
        turboVcResetActive = false;
        turboMoonResetActive = false;
        
        sendFrame();
        delete touchScreen;
        delete remapConfig;
        delete ipManager;
    }

    void updateStatusDisplay() {
        QStringList activeIPs;
        for (const auto& target : ipTargets) {
            if (target.enabled) {
                QString display = target.name.isEmpty() ? target.address : QString("%1 (%2)").arg(target.name, target.address);
                activeIPs.append(display);
            }
        }
        
        if (activeIPs.isEmpty()) {
            statusLabel->setText(tr("No active IP targets"));
            statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        } else {
            QString statusText = tr("Active targets (%1): %2").arg(activeIPs.size()).arg(activeIPs.join(", "));
            statusLabel->setText(statusText);
            statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        switch(event->key())
        {
            case Qt::Key_Tab:
                // Let Qt handle tab navigation
                QWidget::keyPressEvent(event);
                break;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                // Trigger the currently focused button
                if (QPushButton *focusedButton = qobject_cast<QPushButton*>(focusWidget()))
                {
                    focusedButton->click();
                }
                break;
            default:
                QWidget::keyPressEvent(event);
                break;
        }
    }

};


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Widget w;
    GamepadMonitor m(&w);
    FrameTimer t(&w);
    TouchScreen ts;
    t.start(50);
    w.show();

    return a.exec();
}
