/*
 * Author:      Eftychios Papasifakis
 * File:        src/gui/main.cpp
 * Project:     ASUS Keyboard Backlight (asus-keyboard-backlight)
 * SPDX-License-Identifier: MIT
 *
 * Description:
 *   asus-kbd-backlight-gui is a Qt6 window for choosing the keyboard
 *   color and effect and speed and brightness
 *
 *   All hardware access goes through the C API in kbd.h
 *   Every click is applied right away so there is no Apply button
 */
#include <QApplication>
#include <QButtonGroup>
#include <QColorDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include <cerrno>
#include <cstring>
#include <iterator>

#include "kbd.h"

/* Anonymous namespace keeps these names private to this file */
namespace {

/* Colors shown as round swatches from red to white */
const QColor presets[] = {
    QColor("#ff0000"), QColor("#ff6000"), QColor("#ffd000"), QColor("#00ff40"),
    QColor("#00d0ff"), QColor("#0040ff"), QColor("#8000ff"), QColor("#ffffff"),
};

/* Button labels where the index matches the enum values in kbd.h */
const char *const modeNames[] = { "Static", "Breathing", "Color cycle" };
const char *const speedNames[] = { "Slow", "Medium", "Fast" };

/* Names for brightness levels 0 to 3 which TUF laptops use */
const char *const levelNames[] = { "Off", "Low", "Medium", "High" };

/*
 * Qt stylesheet for the dark theme which works like CSS
 * Widgets are matched by class name and by objectName after the hash sign
 * Segment buttons are matched by the segment property set in makeSegments
 */
const char *const appStyle = R"(
    #root { background: #15171c; }
    QLabel { background: transparent; color: #e8eaf0; }
    QLabel#title { font-size: 17pt; font-weight: 600; }
    QLabel#muted { color: #8a91a3; }
    QLabel#section { color: #8a91a3; font-size: 8pt; font-weight: 600; letter-spacing: 1px; }
    QFrame#card { background: #1e2129; border-radius: 14px; }

    QPushButton[segment="true"] {
        background: #2a2e39; color: #e8eaf0; border: none;
        border-radius: 8px; padding: 8px 10px;
    }
    QPushButton[segment="true"]:hover { background: #343947; }
    QPushButton[segment="true"]:checked { background: #4f7cff; color: white; }
    QPushButton[segment="true"]:disabled { background: #22252e; color: #555b69; }

    QSlider::groove:horizontal { height: 6px; background: #2a2e39; border-radius: 3px; }
    QSlider::sub-page:horizontal { background: #4f7cff; border-radius: 3px; }
    QSlider::handle:horizontal {
        background: white; width: 18px; height: 18px; margin: -6px 0; border-radius: 9px;
    }
)";

/* Turn a negative errno from kbd.h into a message for the status line */
QString errorText(int err)
{
    if (err == -EACCES)
        return "Permission denied: install the udev rule (see README).";
    return QString("Error: %1").arg(std::strerror(-err));
}

/*
 * Style for a round color swatch
 * The selected swatch gets a white ring or a blue ring on very light
 * colors where a white ring would be invisible
 */
QString swatchStyle(const QColor &color)
{
    QString ring = color.lightness() > 200 ? "#4f7cff" : "white";
    return QString("QPushButton { background: %1; border: 3px solid #1e2129; border-radius: 18px; }"
                   "QPushButton:hover { border-color: #4a5060; }"
                   "QPushButton:checked { border-color: %2; }")
        .arg(color.name(), ring);
}

/*
 * The whole app is this one window
 * Qt deletes child widgets with their parent so there are no manual deletes
 */
class MainWindow : public QWidget {
public:
    MainWindow()
    {
        /* WA_StyledBackground lets the stylesheet paint a plain QWidget background */
        setObjectName("root");
        setAttribute(Qt::WA_StyledBackground);
        setStyleSheet(appStyle);
        setWindowTitle("ASUS Keyboard Backlight");
        setMinimumWidth(440);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(14);

        /* Header */
        auto *title = new QLabel("Keyboard Backlight");
        title->setObjectName("title");
        auto *subtitle = new QLabel("ASUS TUF RGB keyboard");
        subtitle->setObjectName("muted");
        layout->addWidget(title);
        layout->addWidget(subtitle);

        /* Bar that shows the current color and effect */
        preview = new QLabel;
        preview->setAlignment(Qt::AlignCenter);
        preview->setMinimumHeight(64);
        layout->addWidget(preview);

        layout->addWidget(buildColorCard());
        layout->addWidget(buildEffectCard());
        layout->addWidget(buildBrightnessCard());

        /* Empty unless the last write failed */
        status = new QLabel;
        status->setObjectName("muted");
        status->setWordWrap(true);
        layout->addWidget(status);
        layout->addStretch();

        /*
         * kbd_rgb_mode is write only so the current color cannot be read
         * Start with white selected but do not write it until the user clicks
         */
        colorGroup->button(int(std::size(presets)) - 1)->setChecked(true);
        color = Qt::white;
        refreshView();
    }

private:
    /* A rounded panel with a small uppercase heading that returns its body layout */
    static QVBoxLayout *makeCard(const QString &heading, QFrame **card)
    {
        *card = new QFrame;
        (*card)->setObjectName("card");
        auto *body = new QVBoxLayout(*card);
        body->setContentsMargins(16, 14, 16, 16);
        body->setSpacing(10);
        auto *label = new QLabel(heading.toUpper());
        label->setObjectName("section");
        body->addWidget(label);
        return body;
    }

    /*
     * A row of toggle buttons where only one can be selected
     * Each button id is its array index so it maps straight to the enum value
     */
    QHBoxLayout *makeSegments(const char *const *names, int count, QButtonGroup *group)
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        for (int i = 0; i < count; i++) {
            auto *button = new QPushButton(names[i]);
            button->setProperty("segment", true);
            button->setCheckable(true);
            button->setCursor(Qt::PointingHandCursor);
            group->addButton(button, i);
            row->addWidget(button);
        }
        group->button(0)->setChecked(true);
        return row;
    }

    /* Preset swatches plus a button for a custom color */
    QFrame *buildColorCard()
    {
        QFrame *card;
        QVBoxLayout *body = makeCard("Color", &card);

        /* The button group makes the swatches exclusive so only one shows the ring */
        colorGroup = new QButtonGroup(this);
        auto *row = new QHBoxLayout;
        row->setSpacing(6);

        for (int i = 0; i < int(std::size(presets)); i++) {
            auto *swatch = new QPushButton;
            swatch->setFixedSize(36, 36);
            swatch->setCheckable(true);
            swatch->setCursor(Qt::PointingHandCursor);
            swatch->setToolTip(presets[i].name());
            swatch->setStyleSheet(swatchStyle(presets[i]));
            colorGroup->addButton(swatch, i);
            row->addWidget(swatch);
            connect(swatch, &QPushButton::clicked, this, [this, i] {
                color = presets[i];
                applyColor();
            });
        }

        /* The plus button opens a color picker and then shows the picked color */
        customButton = new QPushButton("+");
        customButton->setFixedSize(36, 36);
        customButton->setCheckable(true);
        customButton->setCursor(Qt::PointingHandCursor);
        customButton->setToolTip("Custom color");
        customButton->setStyleSheet(swatchStyle(QColor("#2a2e39")) +
                                    "QPushButton { color: #e8eaf0; font-size: 14pt; }");
        colorGroup->addButton(customButton);
        row->addWidget(customButton);
        connect(customButton, &QPushButton::clicked, this, [this] { pickCustomColor(); });

        row->addStretch();
        body->addLayout(row);
        return card;
    }

    /* Effect and speed buttons where any click writes the new setting */
    QFrame *buildEffectCard()
    {
        QFrame *card;
        QVBoxLayout *body = makeCard("Effect", &card);

        modeGroup = new QButtonGroup(this);
        body->addLayout(makeSegments(modeNames, 3, modeGroup));

        auto *speedLabel = new QLabel("Speed");
        speedLabel->setObjectName("muted");
        body->addWidget(speedLabel);

        speedGroup = new QButtonGroup(this);
        body->addLayout(makeSegments(speedNames, 3, speedGroup));
        speedGroup->button(KBD_SPEED_MEDIUM)->setChecked(true);

        connect(modeGroup, &QButtonGroup::idClicked, this, [this] { applyColor(); });
        connect(speedGroup, &QButtonGroup::idClicked, this, [this] { applyColor(); });
        return card;
    }

    /* Brightness slider that starts at the level the keyboard has now */
    QFrame *buildBrightnessCard()
    {
        QFrame *card;
        QVBoxLayout *body = makeCard("Brightness", &card);

        /* Fall back to off and 3 if the files cannot be read */
        int level = 0, max = 3;
        kbd_get_brightness(&level);
        kbd_get_max_brightness(&max);

        auto *row = new QHBoxLayout;
        auto *slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, max);
        slider->setValue(level);
        slider->setPageStep(1);
        slider->setCursor(Qt::PointingHandCursor);

        brightnessLabel = new QLabel;
        brightnessLabel->setMinimumWidth(56);
        brightnessLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        setBrightnessLabel(level, max);

        row->addWidget(slider);
        row->addWidget(brightnessLabel);
        body->addLayout(row);

        /* The slider only has a few steps so writing on every change is fine */
        connect(slider, &QSlider::valueChanged, this, [this, max](int value) {
            setBrightnessLabel(value, max);
            showResult(kbd_set_brightness(value));
        });
        return card;
    }

    void pickCustomColor()
    {
        QColor picked = QColorDialog::getColor(color, this, "Custom keyboard color");
        if (!picked.isValid()) {
            /*
             * The dialog was cancelled but the plus button is now selected
             * Select the swatch that matches the current color again if there is one
             */
            for (int i = 0; i < int(std::size(presets)); i++)
                if (presets[i] == color)
                    colorGroup->button(i)->setChecked(true);
            return;
        }
        color = picked;
        customButton->setText("");
        customButton->setStyleSheet(swatchStyle(color));
        applyColor();
    }

    /* Show a name like Low or High or just the number on keyboards with other ranges */
    void setBrightnessLabel(int level, int max)
    {
        bool named = max == int(std::size(levelNames)) - 1;
        brightnessLabel->setText(named ? levelNames[level] : QString::number(level));
    }

    /* Send the selected color and effect and speed to the keyboard */
    void applyColor()
    {
        auto mode = static_cast<kbd_mode>(modeGroup->checkedId());
        auto speed = static_cast<kbd_speed>(speedGroup->checkedId());

        refreshView();
        showResult(kbd_set_color(color.red(), color.green(), color.blue(), mode, speed));
    }

    /* Update the preview bar and grey out the speed buttons for the static effect */
    void refreshView()
    {
        int mode = modeGroup->checkedId();
        for (QAbstractButton *button : speedGroup->buttons())
            button->setEnabled(mode != KBD_MODE_STATIC);

        QString gradient, text;
        if (mode == KBD_MODE_COLOR_CYCLE) {
            /* The keyboard cycles through all colors so show a rainbow */
            gradient = "stop:0 #ff0000, stop:0.25 #ffd000, stop:0.5 #00ff40,"
                       "stop:0.75 #0040ff, stop:1 #8000ff";
            text = "Color cycle";
        } else {
            /* Fade from the color to a darker shade of it */
            gradient = QString("stop:0 %1, stop:1 %2").arg(color.name(), color.darker(250).name());
            text = QString("%1  ·  %2").arg(color.name().toUpper(), modeNames[mode]);
        }

        /* Dark text on light colors and white text on dark colors so it stays readable */
        QString textColor =
            mode != KBD_MODE_COLOR_CYCLE && color.lightness() > 140 ? "#15171c" : "white";
        preview->setStyleSheet(
            QString("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, %1);"
                    "border-radius: 14px; color: %2; font-size: 11pt; font-weight: 600;")
                .arg(gradient, textColor));
        preview->setText(text);
    }

    /* Show the error from the last write or clear the status line on success */
    void showResult(int ret) { status->setText(ret ? errorText(ret) : QString()); }

    QColor color;                  /* color currently selected */
    QLabel *preview;               /* color bar at the top */
    QLabel *status;                /* error messages */
    QLabel *brightnessLabel;       /* level name next to the slider */
    QPushButton *customButton;     /* plus button for a custom color */
    QButtonGroup *colorGroup;      /* swatches and the plus button */
    QButtonGroup *modeGroup;       /* effect buttons */
    QButtonGroup *speedGroup;      /* speed buttons */
};

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    /* Fusion looks the same on every desktop so the stylesheet renders the same everywhere */
    app.setStyle("Fusion");

    /* Exit with a clear message on laptops without the ASUS RGB keyboard driver */
    if (!kbd_is_supported()) {
        QMessageBox::critical(nullptr, "ASUS Keyboard Backlight",
                              "No ASUS RGB keyboard found at " KBD_LED_DIR);
        return 1;
    }

    MainWindow window;
    window.show();
    return app.exec();
}
