/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

#include "config.h"
#include "memory.h"
#include "fs.h"
#include "utils.h"
#include "screen.h"
#include "draw.h"
#include "buttons.h"
#include "pin.h"

bool readConfig(const char *configPath)
{
    if(fileRead(&configData, configPath) != sizeof(cfgData) ||
       memcmp(configData.magic, "CONF", 4) != 0 ||
       configData.formatVersionMajor != CONFIG_VERSIONMAJOR ||
       configData.formatVersionMinor != CONFIG_VERSIONMINOR)
    {
        configData.config = 0;
        return false;
    }

    return true;
}

void writeConfig(const char *configPath, u32 configTemp)
{
    /* If the configuration is different from previously, overwrite it.
       Just the no-forcing flag being set is not enough */
    if((configTemp & 0xFFFFFFEF) != configData.config)
    {
        //Merge the new options and new boot configuration
        configData.config = (configData.config & 0xFFFFFFC0) | (configTemp & 0x3F);

        memcpy(configData.magic, "CONF", 4);
        configData.formatVersionMajor = CONFIG_VERSIONMAJOR;
        configData.formatVersionMinor = CONFIG_VERSIONMINOR;

        if(!fileWrite(&configData, configPath, sizeof(cfgData)))
        {
            createDirectory("luma");
            if(!fileWrite(&configData, configPath, sizeof(cfgData)))
                error("Error writing the configuration file");
        }
    }
}

void configMenu(bool oldPinStatus)
{
    initScreens();

    drawString(CONFIG_TITLE, 10, 10, COLOR_TITLE);
    drawString("Press A to select, START to save", 10, 30, COLOR_WHITE);

    const char *multiOptionsText[]  = { "Screen brightness: 4( ) 3( ) 2( ) 1( )",
                                        "New 3DS CPU: Off( ) Clock( ) L2( ) Clock+L2( )" };

    const char *singleOptionsText[] = { "( ) Autoboot SysNAND",
                                        "( ) Use SysNAND FIRM if booting with R (A9LH)",
                                        "( ) Use second EmuNAND as default",
                                        "( ) Enable region/language emu. and ext. .code",
                                        "( ) Show current NAND in System Settings",
                                        "( ) Enable experimental TwlBg patches",
                                        "( ) Show GBA boot screen in patched AGB_FIRM",
                                        "( ) Display splash screen before payloads",
                                        "( ) Use a PIN" };

    struct multiOption {
        int posXs[4];
        int posY;
        u32 enabled;
    } multiOptions[] = {
        { .posXs = {21, 26, 31, 36} },
        { .posXs = {17, 26, 32, 44} }
    };

    //Calculate the amount of the various kinds of options and pre-select the first single one
    u32 multiOptionsAmount = sizeof(multiOptions) / sizeof(struct multiOption),
        singleOptionsAmount = sizeof(singleOptionsText) / sizeof(char *),
        totalIndexes = multiOptionsAmount + singleOptionsAmount - 1,
        selectedOption = multiOptionsAmount;

    struct singleOption {
        int posY;
        bool enabled;
    } singleOptions[singleOptionsAmount];

    //Parse the existing options
    for(u32 i = 0; i < multiOptionsAmount; i++)
        multiOptions[i].enabled = MULTICONFIG(i);
    for(u32 i = 0; i < singleOptionsAmount; i++)
        singleOptions[i].enabled = CONFIG(i);

    //Character to display a selected option
    char selected = 'x';

    int endPos = 42;

    //Display all the multiple choice options in white
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        multiOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(multiOptionsText[i], 10, multiOptions[i].posY, COLOR_WHITE);
        drawCharacter(selected, 10 + multiOptions[i].posXs[multiOptions[i].enabled] * SPACING_X, multiOptions[i].posY, COLOR_WHITE);
    }

    endPos += SPACING_Y / 2;
    u32 color = COLOR_RED;

    //Display all the normal options in white except for the first one
    for(u32 i = 0; i < singleOptionsAmount; i++)
    {
        singleOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(singleOptionsText[i], 10, singleOptions[i].posY, color);
        if(singleOptions[i].enabled) drawCharacter(selected, 10 + SPACING_X, singleOptions[i].posY, color);
        color = COLOR_WHITE;
    }

    u32 pressed = 0;

    //Boring configuration menu
    while(pressed != BUTTON_START)
    {
        do
        {
            pressed = waitInput();
        }
        while(!(pressed & MENU_BUTTONS));

        if(pressed != BUTTON_A)
        {
            //Remember the previously selected option
            u32 oldSelectedOption = selectedOption;

            switch(pressed)
            {
                case BUTTON_UP:
                    selectedOption = !selectedOption ? totalIndexes : selectedOption - 1;
                    break;
                case BUTTON_DOWN:
                    selectedOption = selectedOption == totalIndexes ? 0 : selectedOption + 1;
                    break;
                case BUTTON_LEFT:
                    selectedOption = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedOption = totalIndexes;
                    break;
                default:
                    continue;
            }

            if(selectedOption == oldSelectedOption) continue;

            //The user moved to a different option, print the old option in white and the new one in red. Only print 'x's if necessary
            if(oldSelectedOption < multiOptionsAmount)
            {
                drawString(multiOptionsText[oldSelectedOption], 10, multiOptions[oldSelectedOption].posY, COLOR_WHITE);
                drawCharacter(selected, 10 + multiOptions[oldSelectedOption].posXs[multiOptions[oldSelectedOption].enabled] * SPACING_X, multiOptions[oldSelectedOption].posY, COLOR_WHITE);
            }
            else
            {
                u32 singleOldSelected = oldSelectedOption - multiOptionsAmount;
                drawString(singleOptionsText[singleOldSelected], 10, singleOptions[singleOldSelected].posY, COLOR_WHITE);
                if(singleOptions[singleOldSelected].enabled) drawCharacter(selected, 10 + SPACING_X, singleOptions[singleOldSelected].posY, COLOR_WHITE);
            }

            if(selectedOption < multiOptionsAmount)
                drawString(multiOptionsText[selectedOption], 10, multiOptions[selectedOption].posY, COLOR_RED);
            else
            {
                u32 singleSelected = selectedOption - multiOptionsAmount;
                drawString(singleOptionsText[singleSelected], 10, singleOptions[singleSelected].posY, COLOR_RED);
            }
        }
        else
        {
            //The selected option's status changed, print the 'x's accordingly
            if(selectedOption < multiOptionsAmount)
            {
                u32 oldEnabled = multiOptions[selectedOption].enabled;
                drawCharacter(selected, 10 + multiOptions[selectedOption].posXs[oldEnabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_BLACK);
                multiOptions[selectedOption].enabled = oldEnabled == 3 ? 0 : oldEnabled + 1;

                if(!selectedOption)
                    updateBrightness(multiOptions[selectedOption].enabled);
            }
            else
            {
                bool oldEnabled = singleOptions[selectedOption - multiOptionsAmount].enabled;
                singleOptions[selectedOption - multiOptionsAmount].enabled = !oldEnabled;
                if(oldEnabled) drawCharacter(selected, 10 + SPACING_X, singleOptions[selectedOption - multiOptionsAmount].posY, COLOR_BLACK);
            }
        }

        //In any case, if the current option is enabled (or a multiple choice option is selected) we must display a red 'x'
        if(selectedOption < multiOptionsAmount)
            drawCharacter(selected, 10 + multiOptions[selectedOption].posXs[multiOptions[selectedOption].enabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_RED);
        else
        {
            u32 singleSelected = selectedOption - multiOptionsAmount;
            if(singleOptions[singleSelected].enabled) drawCharacter(selected, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_RED);
        }
    }

    //Preserve the last-used boot options (last 12 bits)
    configData.config &= 0x3F;

    //Parse and write the new configuration
    for(u32 i = 0; i < multiOptionsAmount; i++)
        configData.config |= multiOptions[i].enabled << (i * 2 + 6);
    for(u32 i = 0; i < singleOptionsAmount; i++)
        configData.config |= (singleOptions[i].enabled ? 1 : 0) << (i + 16);

    if(CONFIG(8)) newPin(oldPinStatus);
    else if(oldPinStatus) fileDelete("/luma/pin.bin");

    //Wait for the pressed buttons to change
    while(HID_PAD & PIN_BUTTONS);

    chrono(2);
}