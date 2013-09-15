/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 2013 Unvanquished Developers

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "BaseCommands.h"

#include <vector>
#include <unordered_map>

#include "CommandSystem.h"
#include "CvarSystem.h"
#include "../../shared/Command.h"
#include "../../shared/String.h"

/**
 * Definition of most of the commands that provide scripting capabilities to the console
 */

namespace Cmd {

    class VstrCmd: public StaticCmd {
        public:
            VstrCmd(): StaticCmd("vstr", BASE, N_("executes a variable command")) {
            }

            void Run(const Cmd::Args& args) const override {
                if (args.Argc() != 2) {
                    PrintUsage(args, _("<variablename>"), _("executes a variable command"));
                    return;
                }

                std::string command = Cvar::GetValue(args.Argv(1));
                Cmd::BufferCommandText(command, Cmd::AFTER, true);
            }

            std::vector<std::string> Complete(int pos, const Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum == 1) {
                    return Cvar::CompleteName(args.ArgPrefix(pos));
                }

                return {};
            }
    };
    static VstrCmd VstrCmdRegistration;

    class ExecCmd: public StaticCmd {
        public:
            ExecCmd(const std::string& name, bool silent): StaticCmd(name, BASE, N_("executes a command file")), silent(silent) {
            }

            void Run(const Cmd::Args& args) const override {
                bool executeSilent = silent;
                bool failSilent = false;
                bool hasOptions = args.Argc() >= 3 and args.Argv(1).size() >= 2 and args.Argv(1)[0] == '-';

                //Check the syntax
                if (args.Argc() < 2) {
                    if (silent) {
                        PrintUsage(args, _("<filename> [<arguments>…]"), _("execute a script file without notification, a shortcut for exec -q"));
                    } else {
                        PrintUsage(args, _("[-q|-f|-s] <filename> [<arguments>…]"), _("execute a script file."));
                    }
                    return;
                }

                //Read options
                if (hasOptions) {
                    switch (args.Argv(1)[1]) {
                        case 'q':
                            executeSilent = true;
                            break;

                        case 'f':
                            failSilent = true;
                            break;

                        case 's':
                            executeSilent = failSilent = true;
                            break;

                        default:
                            break;
                    }
                }

                int filenameArg = hasOptions ? 2 : 1;
                const std::string& filename = args.Argv(filenameArg);

                SetExecArgs(args, filenameArg + 1);
                if (not ExecFile(filename)) {
                    if (not failSilent) {
                        Com_Printf(_("couldn't exec '%s'\n"), filename.c_str());
                    }
                    return;
                }

                if (not executeSilent) {
                    Com_Printf(_("execing '%s'\n"), filename.c_str());
                }
            }

        private:
            bool silent;

            void SetExecArgs(const Cmd::Args& args, int start) const {
                //Set some cvars up so that scripts file can be used like functions
                Cvar_Get("arg_all", args.RawArgsFrom(start).c_str(), CVAR_TEMP | CVAR_ROM | CVAR_USER_CREATED);
                Cvar_Set("arg_all", args.RawArgsFrom(start).c_str());
                Cvar_Get("arg_count", va("%i", args.Argc() - start), CVAR_TEMP | CVAR_ROM | CVAR_USER_CREATED);
                Cvar_Set("arg_count", va("%i", args.Argc() - start));

                for (int i = start; i < args.Argc(); i++) {
                    Cvar_Get(va("arg_%i", i), args.Argv(i + start - 1).c_str(), CVAR_TEMP | CVAR_ROM | CVAR_USER_CREATED);
                    Cvar_Set(va("arg_%i", i), args.Argv(i + start - 1).c_str());
                }
            }

            bool ExecFile(const std::string& filename) const {
                bool success = false;
                fileHandle_t h;
                int len = FS_SV_FOpenFileRead(filename.c_str(), &h);

                //TODO: rewrite this
                if (h) {
                    success = true;
                    char* content = (char*) Hunk_AllocateTempMemory(len + 1);
                    FS_Read(content, len, h);
                    content[len] = '\0';
                    FS_FCloseFile(h);
                    Cmd::BufferCommandText(content, Cmd::AFTER, true);
                    Hunk_FreeTempMemory(content);
                } else {
                    char* content;
                    FS_ReadFile(filename.c_str(), (void**) &content);

                    if (content) {
                        success = true;
                        Cmd::BufferCommandText(content, Cmd::AFTER, true);
                        FS_FreeFile(content);
                    }
                }
                return success;
            }
    };
    static ExecCmd ExecCmdRegistration("exec", false);
    static ExecCmd ExecqCmdRegistration("execq", true);

    class EchoCmd: public StaticCmd {
        public:
            EchoCmd(): StaticCmd("echo", BASE, N_("prints to the console")) {
            }

            void Run(const Cmd::Args& args) const override {
                for (int i = 1; i < args.Argc(); i++) {
                    Com_Printf("%s", args.Argv(i).c_str());
                }
                Com_Printf("\n");
            }
    };
    static EchoCmd EchoCmdRegistration;

    class RandomCmd: public StaticCmd {
        public:
            RandomCmd(): StaticCmd("random", BASE, N_("sets a variable to a random integer")) {
            }

            void Run(const Cmd::Args& args) const override {
                if (args.Argc() != 4) {
                    PrintUsage(args, _("<variableToSet> <minNumber> <maxNumber>"), _("sets a variable to a random integer between minNumber and maxNumber"));
                    return;
                }

                const std::string& cvar = args.Argv(1);
                int min = Str::ToInt(args.Argv(2));
                int max = Str::ToInt(args.Argv(3));
                int number = min + (std::rand() % (max - min));

                Cvar::SetValue(cvar, std::to_string(number));
            }

            std::vector<std::string> Complete(int pos, const Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum == 1) {
                    return Cvar::CompleteName(args.ArgPrefix(pos));
                }

                return {};
            }
    };
    static RandomCmd RandomCmdRegistration;

    class ConcatCmd: public StaticCmd {
        public:
            ConcatCmd(): StaticCmd("concat", BASE, N_("concatenatas variables")) {
            }

            void Run(const Cmd::Args& args) const override {
                if (args.Argc() < 3) {
                    PrintUsage(args, _("<variableToSet> <variable1> … <variableN>"), _("concatenates variable1 to variableN and sets the result to variableToSet"));
                    return;
                }

                std::string res;
                for (int i = 2; i < args.Argc(); i++) {
                    res += Cvar::GetValue(args.Argv(i));
                }
                Cvar::SetValue(args.Argv(1), res);
            }

            std::vector<std::string> Complete(int pos, const Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum >= 1) {
                    return Cvar::CompleteName(args.ArgPrefix(pos));
                }

                return {};
            }
    };
    static ConcatCmd ConcatCmdRegistration;

    class MathCmd: public StaticCmd {
        public:
            MathCmd(): StaticCmd("math", BASE, N_("does math and sets the result to a variable")) {
            }

            void Run(const Cmd::Args& args) const override {
                if  (args.Argc() < 2) {
                    Usage(args);
                    return;
                }

                const std::string& targetName = args.Argv(1);
                float currentValue = atof(Cvar::GetValue(targetName).c_str()); //TODO: Str::ToFloat
                float newValue;

                if (args.Argc() == 3) {
                    // only one additional argument (++, --)
                    const std::string& op = args.Argv(2);

                    if (op == "++") {
                        newValue = currentValue + 1;
                    } else if (op == "--") {
                        newValue = currentValue - 1;
                    } else {
                        Usage(args);
                        return;
                    }

                } else if (args.Argc() == 4) {
                    // two additional arguments (+=, -=, *=, /=)
                    const std::string& op = args.Argv(2);
                    float operand = std::stof(args.Argv(3));

                    if (op == "+") {
                        newValue = currentValue + operand;
                    } else if (op == "-") {
                        newValue = currentValue - operand;
                    } else if (op == "*") {
                        newValue = currentValue * operand;
                    } else if (op == "/" or op == "÷") {
                        if (operand == 0.0f) {
                            Com_Log(LOG_ERROR, _( "Cannot divide by 0!" ));
                            return;
                        }
                        newValue = currentValue / operand;
                    } else {
                        Usage(args);
                        return;
                    }

                } else if (args.Argc() == 6) {
                    // for additional arguments (_ = _ op _ with op in +, -, *, /)
                    float operand1 = std::stof(args.Argv(3));
                    const std::string& op = args.Argv(4);
                    float operand2 = std::stof(args.Argv(5));

                    if (op == "+") {
                        newValue = operand1 + operand2;
                    } else if (op == "-") {
                        newValue = operand1 - operand2;
                    } else if (op == "*") {
                        newValue = operand1 * operand2;
                    } else if (op == "/" or op == "÷") {
                        if (operand2 == 0.0f) {
                            Com_Log(LOG_ERROR, _( "Cannot divide by 0!" ));
                            return;
                        }
                        newValue = operand1 / operand2;
                    } else {
                        Usage(args);
                        return;
                    }

                } else {
                    Usage(args);
                    return;
                }

                Cvar::SetValue(targetName, std::to_string(newValue));
            }

            void Usage(const Cmd::Args& args) const{
                PrintUsage(args, _("<variableToSet> = <number> <operator> <number>"), "");
                PrintUsage(args, _("<variableToSet> <operator> <number>"), "");
                PrintUsage(args, _("<variableToSet> (++|--)"), "");
                Com_Printf(_("valid operators: + - × * ÷ /\n"));
            }

            std::vector<std::string> Complete(int pos, const Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum == 1) {
                    return Cvar::CompleteName(args.ArgPrefix(pos));
                }

                return {};
            }
    };
    static MathCmd MathCmdRegistration;

    class IfCmd: public StaticCmd {
        public:
            IfCmd(): StaticCmd("if", BASE, N_("conditionnaly execute commands")) {
            }

            void Run(const Cmd::Args& args) const override {
                if (args.Argc() != 5 and args.Argc() != 6) {
                    Usage(args);
                    return;
                }

                const std::string& value1 = args.Argv(1);
                const std::string& value2 = args.Argv(3);
                const std::string& relation = args.Argv(2);

                int intValue1 = Str::ToInt(value1);
                int intValue2 = Str::ToInt(value2);

                bool result;

                if (relation == "=" or relation == "==") {
                    result = intValue1 = intValue2;

                } else if (relation == "!=" or relation == "≠") {
                    result = intValue1 != intValue2;

                } else if (relation == "<") {
                    result = intValue1 < intValue2;

                } else if (relation == "<=" or relation == "≤" ) {
                    result = intValue1 <= intValue2;

                } else if (relation == ">") {
                    result = intValue1 > intValue2;

                } else if (relation == ">=" or relation == "≥") {
                    result = intValue1 >= intValue2;

                } else if (relation == "eq") {
                    result = value1 == value2;

                } else if (relation == "ne") {
                    result = value1 != value2;

                } else if (relation == "in") {
                    result = value2.find(value1) != std::string::npos;

                } else if (relation == "!in") {
                    result = value2.find(value1) == std::string::npos;

                } else {
                    Com_Printf(_( "invalid relation operator in if command. valid relation operators are = != ≠ < > ≥ >= ≤ <= eq ne in !in\n" ));
                    Usage(args);
                    return;
                }

                const std::string& toRun = result ? args.Argv(4) : args.Argv(5);

                //if it starts with / or \ it is a quoted command
                if (toRun.size() > 0 and (toRun[0] == '/' or toRun[0] == '\\')) {
                    Cmd::BufferCommandText(toRun.c_str() + 1, Cmd::AFTER, true);

                } else {
                    std::string command = Cvar::GetValue(toRun);
                    Cmd::BufferCommandText(command, Cmd::AFTER, true);
                }
            }

            void Usage(const Cmd::Args& args) const{
                PrintUsage(args, _("if <number|string> <relation> <number|string> <cmdthen> (<cmdelse>)"), "compares two numbers or two strings and executes <cmdthen> if true, <cmdelse> if false\n");
                Com_Printf(_("-- commands are cvar names unless prefixed with / or \\\n"));
            }

            std::vector<std::string> Complete(int pos, const Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum == 4 or argNum == 5) {
                    return Cvar::CompleteName(args.ArgPrefix(pos));
                }

                return {};
            }
    };
    static IfCmd IfCmdRegistration;

    class ToggleCmd: public Cmd::StaticCmd {
        public:
            ToggleCmd(): Cmd::StaticCmd("toggle", Cmd::BASE, N_("toggles a cvar between different values")) {
            }

            void Run(const Cmd::Args& args) const override {
                if (args.Argc() < 2) {
                    Usage(args);
                    return;
                }

                const std::string& firstArg = args.Argv(1);

                int listStart = 2;
                int direction = 1;

                if (firstArg == "+") {
                    listStart = 3;
                } else if (firstArg == "-") {
                    listStart = 3;
                    direction = -1;
                }

                if (args.Argc() < listStart) {
                    Usage(args);
                    return;
                }

                const std::string& name = args.Argv(listStart - 1);

                if (args.Argc() == listStart) {
                    //There is no list, toggle between 0 and 1
                    Cvar::SetValue(name, va("%d", !Str::ToInt(Cvar::GetValue(name)))); //TODO: use Cvar::ParseValue(bool)
                    return;
                }

                //Toggle the cvar through a list of values
                std::string currentValue = Cvar::GetValue(name);

                for(int i = listStart; i < args.Argc(); i++) {
                    if(currentValue == args.Argv(i)) {
                        //Found the current value, choose the next one
                        int next = (i + direction) % (args.Argc() - listStart);

                        Cvar::SetValue(name, args.Argv(next + listStart));
                        return;
                    }
                }

                //fallback
                Cvar::SetValue(name, args.Argv(listStart));
            }

            std::vector<std::string> Complete(int pos, const Cmd::Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum == 1 or argNum == 2) {
                    return Cvar::CompleteName(args.ArgPrefix(pos));
                }

                return {};
            }

            void Usage(const Cmd::Args& args) const{
                PrintUsage(args, _("[+|-] <variable> [<value>…]"), "");
            }
    };
    static ToggleCmd ToggleCmdRegistration;

    class CycleCmd: public Cmd::StaticCmd {
        public:
            CycleCmd(): Cmd::StaticCmd("cycle", Cmd::BASE, N_("cycles a cvar through numbers")) {
            }

            void Run(const Cmd::Args& args) const override {
                if (args.Argc() < 4 || args.Argc() > 5) {
                    PrintUsage(args, _("<variable> <start> <end> [<step>]"), "");
                    return;
                }

                int oldValue = Str::ToInt(Cvar::GetValue(args.Argv(1)));
                int start = Str::ToInt(args.Argv(2));
                int end = Str::ToInt(args.Argv(3));

                int step;
                if (args.Argc() == 5) {
                    step = Str::ToInt(args.Argv(4));
                } else {
                    step = 1;
                }
                if (std::abs(end - start) < step) {
                    step = 1;
                }

                //TODO: rewrite all this nonsense
                int newValue;
                if (end < start) {
                    newValue = oldValue - step;
                    if (newValue < start) {
                        newValue = start - (step - (oldValue - end + 1));
                    }
                } else {
                    newValue = oldValue + step;
                    if (newValue > end) {
                        newValue = start + (step - (end - oldValue + 1));
                    }
                }

                Cvar::SetValue(args.Argv(1), va("%i", newValue));
            }

            std::vector<std::string> Complete(int pos, const Cmd::Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum == 1) {
                    return Cvar::CompleteName(args.ArgPrefix(pos));
                }

                return {};
            }
    };
    static CycleCmd CycleCmdRegistration;

    /*
    ===============================================================================

    Cmd:: delay related commands

    ===============================================================================
    */

    enum delayType_t {
        MSEC,
        FRAME
    };

    struct delayRecord_t {
        std::string name;
        std::string command;
        int target;
        delayType_t type;
    };

    static std::vector<delayRecord_t> delays;
    static int delayFrame = 0;

    void DelayFrame() {
        int time = Sys_Milliseconds();
        delayFrame ++;

        for (auto it = delays.begin(); it != delays.end();) {
            const delayRecord_t& delay = *it;

            if ((delay.type == MSEC and delay.target <= time) or (delay.type == FRAME and delay.target < delayFrame)) {
                Cmd::BufferCommandText(delay.command, Cmd::END, true);
                it = delays.erase(it);
            } else {
                it ++;
            }
        }
    }

    std::vector<std::string> CompleteDelayName(const std::string& prefix) {
        std::vector<std::string> res;

        for (auto& delay: delays) {
            if (Str::IsPrefix(prefix, delay.name)) {
                res.push_back(delay.name);
            }
        }

        return res;
    }

    class DelayCmd: public StaticCmd {
        public:
            DelayCmd(): StaticCmd("delay", BASE, N_("executes a command after a delay")) {
            }

            void Run(const Cmd::Args& args) const override {
                int argc = args.Argc();

                if (argc < 3 or argc > 4) {
		            PrintUsage(args, _( "delay (name) <delay in milliseconds> <command>\n  delay <delay in frames>f <command>"), _("executes <command> after the delay\n" ));
		            return;
                }

                //Get all the parameters!
                const std::string& command = args.Argv(argc - 1);
                std::string delay = args.Argv(argc - 2);
                int target = Str::ToInt(delay);
                const std::string& name = (argc == 4) ? args.Argv(1) : "";
                delayType_t type;

                if (target < 1) {
                    Com_Printf(_("delay: the delay must be a positive integer\n"));
                    return;
                }

                if (delay[delay.size() - 1] == 'f') {
                    target += delayFrame;
                    type = FRAME;
                } else {
                    target += Sys_Milliseconds();
                    type = MSEC;
                }

                delays.emplace_back(delayRecord_t{name, command, target, type});
            }

            std::vector<std::string> Complete(int pos, const Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum == 1) {
                    return CompleteDelayName(args.ArgPrefix(pos));
                }

                return {};
            }
    };

    class UndelayCmd: public StaticCmd {
        public:
            UndelayCmd(): StaticCmd("undelay", BASE, N_("removes named /delay commands")) {
            }

            void Run(const Cmd::Args& args) const override {
                if (args.Argc() < 2) {
                    PrintUsage(args, _("<name> (command)"), _( "removes all commands with <name> in them.\nif (command) is specified, the removal will be limited only to delays whose commands contain (command)."));
                    return;
                }

                const std::string& name = args.Argv(1);
                const std::string& command = (args.Argc() >= 3) ? args.Argv(2) : "";

                for (auto it = delays.begin(); it != delays.end();) {
                    const delayRecord_t& delay = *it;

                    if (Q_stristr(delay.name.c_str(), name.c_str()) and Q_stristr(delay.command.c_str(), command.c_str())) {
                        it = delays.erase(it);
                    } else {
                        it ++;
                    }
                }
            }

            std::vector<std::string> Complete(int pos, const Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum == 1) {
                    return CompleteDelayName(args.ArgPrefix(pos));
                }

                return {};
            }
    };

    class UndelayAllCmd: public StaticCmd {
        public:
            UndelayAllCmd(): StaticCmd("undelayAll", BASE, N_("removes all the pending /delay commands")) {
            }

            void Run(const Cmd::Args& args) const override {
                delays.clear();
            }
    };

    static DelayCmd DelayCmdRegistration;
    static UndelayCmd UndelayCmdRegistration;
    static UndelayAllCmd UndelayAllCmdRegistration;

    /*
    ===============================================================================

    Cmd:: alias related commands

    ===============================================================================
    */

    struct aliasRecord_t {
        std::string command;
        int lastRun;
    };

    static std::unordered_map<std::string, aliasRecord_t> aliases;
    static int aliasRun;
    static bool inAliasRun;

    void WriteAliases(fileHandle_t f) {
        std::string toWrite = "clearAliases\n";
        FS_Write(toWrite.c_str(), toWrite.size(), f);

        for (auto it: aliases) {
            toWrite = "alias " + it.first + " " + it.second.command + "\n";
            FS_Write(toWrite.c_str(), toWrite.size(), f);
        }
    }

    std::vector<std::string> CompleteAliasName(const std::string& prefix) {
        std::vector<std::string> res;

        for (auto it: aliases) {
            if (Str::IsPrefix(prefix, it.first)) {
                res.push_back(it.first);
            }
        }

        return res;
    }

    class AliasProxy: public CmdBase {
        public:
            AliasProxy(): CmdBase(0) {
            }

            void Run(const Cmd::Args& args) const override{
                const std::string& name = args.Argv(0);
                const std::string& parameters = args.RawArgsFrom(1);

                if (aliases.count(name) == 0) {
                    Com_Printf("alias %s doesn't exist", name.c_str());
                    return;
                }

                aliasRecord_t& alias = aliases[name];

                bool startsRun = not inAliasRun;
                if (startsRun) {
                    inAliasRun = true;
                    aliasRun ++;
                }

                if (alias.lastRun == aliasRun) {
                    Com_Printf("recursive alias call at alias %s", name.c_str());
                } else {
                    alias.lastRun = aliasRun;
                    Cmd::BufferCommandText(alias.command + " " + parameters, NOW, true);
                }

                if (startsRun) {
                    inAliasRun = false;
                }
            }
    };
    static AliasProxy aliasProxy;

    class AliasCmd: StaticCmd {
        public:
            AliasCmd(): StaticCmd("alias", BASE, N_("creates or view an alias")) {
            }

            void Run(const Cmd::Args& args) const override{
                if (args.Argc() < 2) {
                    PrintUsage(args, _("<name>"), _("show an alias"));
                    PrintUsage(args, _("<name> <exec>"), _("create an alias"));
                    return;
                }

                const std::string& name = args.Argv(1);

                //Show an alias
                if (args.Argc() == 2) {
                    if (aliases.count(name)) {
                        Com_Printf("%s ⇒ %s\n", name.c_str(), aliases[name].command.c_str());
                    } else {
                        Com_Printf(_("Alias %s does not exist\n"), name.c_str());
                    }
                    return;
                }

                //Modify or create an alias
                const std::string& command = args.RawArgsFrom(2);

                if (aliases.count(name)) {
                    aliases[name].command = command;
                    return;
                }

                if (CommandExists(name)) {
                    Com_Printf(_("Can't override a builtin function with an alias\n"));
                    return;
                }

                aliases[name] = aliasRecord_t{command, aliasRun};
                AddCommand(name, aliasProxy, N_("a user-defined alias command"));

                //Force an update of autogen.cfg (TODO: get rid of this super global variable)
                cvar_modifiedFlags |= CVAR_ARCHIVE;
            }

            std::vector<std::string> Complete(int pos, const Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum == 1) {
                    return CompleteAliasName(args.ArgPrefix(pos));
                } else if (argNum > 1) {
                    return Cmd::CompleteArgument(args.RawArgsFrom(2), pos - args.ArgStartPos(2));
                }

                return {};
            }
    };
    static AliasCmd AliasCmdRegistration;

    class UnaliasCmd: StaticCmd {
        public:
            UnaliasCmd(): StaticCmd("unalias", BASE, N_("deletes an alias")) {
            }

            void Run(const Cmd::Args& args) const override{
                if (args.Argc() != 2) {
                    PrintUsage(args, _("<name>"), _("delete an alias"));
                    return;
                }

                const std::string& name = args.Argv(1);

                if (aliases.count(name)) {
                    aliases.erase(name);
                    RemoveCommand(name);
                }
            }

            std::vector<std::string> Complete(int pos, const Args& args) const override{
                int argNum = args.PosToArg(pos);

                if (argNum == 1) {
                    return CompleteAliasName(args.ArgPrefix(pos));
                }

                return {};
            }
    };
    static UnaliasCmd UnaliasCmdRegistration;

    class ClearAliasesCmd: StaticCmd {
        public:
            ClearAliasesCmd(): StaticCmd("clearAliases", BASE, N_("deletes all the aliases")) {
            }

            void Run(const Cmd::Args& args) const override{
                RemoveFlaggedCommands(ALIAS);
                aliases.clear();
            }
    };
    static ClearAliasesCmd ClearAliasesCmdRegistration;

    class ListAliasesCmd: StaticCmd {
        public:
            ListAliasesCmd(): StaticCmd("listAliases", BASE, N_("lists aliases")) {
            }

            void Run(const Cmd::Args& args) const override{
                std::string name;

                if (args.Argc() > 1) {
                    name = args.Argv(1);
                } else {
                    name = "";
                }

                std::vector<const aliasRecord_t*> matches;
                std::vector<const std::string*> matchesNames;
                int maxNameLength = 0;

                //Find all the matching aliases and their names
                for (auto it = aliases.cbegin(); it != aliases.cend(); ++it) {
                    if (Q_stristr(it->first.c_str(), name.c_str())) {
                        matches.push_back(&it->second);
                        matchesNames.push_back(&it->first);
                        maxNameLength = MAX(maxNameLength, it->first.size());
                    }
                }

                //Print the matches, keeping the description aligned
                for (int i = 0; i < matches.size(); i++) {
                    int toFill = maxNameLength - matchesNames[i]->size();
                    Com_Printf("  %s%s ⇒ %s\n", matchesNames[i]->c_str(), std::string(toFill, ' ').c_str(), matches[i]->command.c_str());
                }

                Com_Printf("%zu aliases\n", matches.size());
            }
    };
    static ListAliasesCmd ListAliasesCmdRegistration;

}