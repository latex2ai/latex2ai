// -----------------------------------------------------------------------------
// MIT License
//
// Copyright (c) 2020 Ivo Steinbrecher
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// -----------------------------------------------------------------------------

/**
 * \brief Errors for l2a applications.
 */


#include "IllustratorSDK.h"

#include "l2a_global.h"

#include "l2a_constants.h"
#include "l2a_error.h"
#include "l2a_execute.h"
#include "l2a_file_system.h"
#include "l2a_latex.h"
#include "l2a_parameter_list.h"
#include "l2a_plugin.h"
#include "l2a_string_functions.h"
#include "l2a_suites.h"
#include "l2a_version.h"

/**
 * Set the global variables to a null pointer
 */
L2A::GLOBAL::Global* L2A::GLOBAL::_l2a_global = nullptr;
L2APlugin* L2A::GLOBAL::_l2a_plugin = nullptr;


/**
 *
 */
L2A::GLOBAL::Global::Global() : is_testing_(false)
{
    // Check if a new version of LaTeX2AI is available. Do this at the beginning in case there is an error in the set
    // and get path functions later on and it is fixed in a future release.
    L2A::GLOBAL::CheckGithubVersion();

    // Set the default parameters.
    std::shared_ptr<L2A::UTIL::ParameterList> default_parameter_list = std::make_shared<L2A::UTIL::ParameterList>();
    GetDefaultParameterList(default_parameter_list);
    SetFromParameterList(*default_parameter_list);

    // Load the parameter list stored in the application data directory.
    {
        // Application data path.
        ai::FilePath application_data_directory = L2A::UTIL::GetApplicationDataDirectory();
        L2A::UTIL::CreateDirectoryL2A(application_data_directory);
        application_data_path_ = application_data_directory;
        application_data_path_.AddComponent(ai::UnicodeString("LaTeX2AI_application_data.xml"));

        //! File with last input for item.
        l2a_item_last_input_ = application_data_directory;
        l2a_item_last_input_.AddComponent(ai::UnicodeString("LaTeX2AI_last_input.xml"));

        if (L2A::UTIL::IsFile(application_data_path_))
        {
            // Try to load the data from the xml file.
            const ai::UnicodeString xml_string = L2A::UTIL::ReadFileUTF8(application_data_path_);
            try
            {
                L2A::UTIL::ParameterList data_list(xml_string);
                if (!SetFromParameterList(data_list))
                {
                    // Could not set all parameters -> warn the user about it.
                    sAIUser->WarningAlert(
                        ai::UnicodeString(
                            "Not all Plug-In settings of LaTeX2AI could be loaded. This can happen if a new version is "
                            "used. Please check if all options are set to your preferences."),
                        nullptr);
                }
            }
            catch (L2A::ERR::Exception&)
            {
                // Something went wrong, remove the application file and continue with the default values.
                L2A::UTIL::RemoveFile(application_data_path_);
            }
        }
    }

    // Clean the temporary directory.
    L2A::UTIL::ClearTemporaryDirectory();

    // Make sure the ghostscript command is valid.
    if (!CheckGhostscriptCommand(gs_command_))
    {
        // The path from the application data file is not valid. Try to automatically find it.
        ai::UnicodeString gs_command = L2A::UTIL::GetGhostScriptCommand();

        // "Officially" set the forms path and check if it is valid.
        if (!SetGhostscriptCommand(gs_command)) return;
    }

    // Make sure the latex path is valid.
    if (!CheckLatexCommand(latex_bin_path_))
    {
        // The path from the application data file is not valid. Try the default value.
        latex_bin_path_ = ai::FilePath(ai::UnicodeString(""));

        // "Officially" set the latex path and check if it is valid.
        if (!SetLatexCommand(latex_bin_path_)) return;
    }
}


/**
 *
 */
L2A::GLOBAL::Global::~Global()
{
    // Save the options in a file.
    L2A::UTIL::WriteFileUTF8(application_data_path_, ToString(), true);
}


/**
 *
 */
bool L2A::GLOBAL::Global::SetGhostscriptCommand(ai::UnicodeString gs_command)
{
    ai::FilePath gs_path(gs_command);

    // Check until the command is correct or the user cancels the operation.
    while (!CheckGhostscriptCommand(gs_command))
    {
        AIBoolean form_result = true;
        ai::UnicodeString form_string(
            "The path to the ghostscript executable (gswin32c.exe, gswin64c.exe) seems to be wrong. Please select the "
            "correct path, otherwise LaTeX2AI can not be used!");
        form_result = sAIUser->OKCancelAlert(form_string, true, nullptr);
        if (!form_result) return false;

        // Ask the user to pick the file.
        AIFileDialogFilters options;
        options.AddFilter(ai::UnicodeString("Executable (*.exe)"), ai::UnicodeString("*.exe"));
        AIErr err =
            sAIUser->GetFileDialog(ai::UnicodeString("Select *.exe for the forms Application"), &options, gs_path);
        if (err == kCanceledErr) return false;
        l2a_check_ai_error(err);

        gs_command = gs_path.GetFullPath();
    }

    gs_command_ = gs_command;
    return true;
}

/**
 *
 */
bool L2A::GLOBAL::Global::CheckGhostscriptCommand(const ai::UnicodeString& gs_command) const
{
    ai::UnicodeString full_gs_command = "\"" + gs_command + "\" -v";
    ai::UnicodeString command_output;

    try
    {
        auto command_result = L2A::UTIL::ExecuteCommandLine(full_gs_command, true);
        if (command_result.output_.find(ai::UnicodeString(" Ghostscript ")) != std::string::npos)
            return true;
        else
            return false;
    }
    catch (...)
    {
        return false;
    }
}

/**
 *
 */
bool L2A::GLOBAL::Global::SetLatexCommand(const ai::FilePath& latex_path)
{
#ifdef WIN_ENV
    ai::FilePath path = latex_path;
#else
    ai::FilePath path = ai::FilePath(ai::UnicodeString("/Library/TeX/texbin"));
#endif

    // Check until the command is correct or the user cancels the operation.
    while (!CheckLatexCommand(path))
    {
        AIBoolean form_result = true;
        ai::UnicodeString form_string(
            "The path to the folder with the LaTeX executables (pdflatex.exe, ...) seems to be wrong. Please select "
            "the correct path, otherwise LaTeX2AI can not be used!");
        form_result = sAIUser->OKCancelAlert(form_string, true, nullptr);
        if (!form_result) return false;

        // Ask the user to pick the directory.
        ai::UnicodeString message("Select *.exe for the forms Application");
        AIErr err = sAIUser->GetDirectoryDialog(message, path);

        if (err == kCanceledErr) return false;
        l2a_check_ai_error(err);
    }

    latex_bin_path_ = path;
    return true;
}

/**
 *
 */
bool L2A::GLOBAL::Global::CheckLatexCommand(const ai::FilePath& path_latex) const
{
    ai::UnicodeString command_latex;
    if (L2A::UTIL::IsDirectory(path_latex))
    {
        ai::FilePath exe_path = path_latex;
#ifdef WIN_ENV
        exe_path.AddComponent(ai::UnicodeString("pdflatex.exe"));
#else
        exe_path.AddComponent(ai::UnicodeString("pdflatex"));
#endif
        command_latex = "\"" + exe_path.GetFullPath() + "\"";
    }
    else if (path_latex.IsEmpty())
        command_latex = ai::UnicodeString("pdflatex");
    else
        // The directory does not exist and is not empty -> this will not work.
        return false;

    command_latex += " -version";
    ai::UnicodeString command_output;
    try
    {
        auto command_result = L2A::UTIL::ExecuteCommandLine(command_latex, true);
        if (command_result.output_.find(ai::UnicodeString("pdfTeX")) != std::string::npos)
            return true;
        else
            return false;
    }
    catch (...)
    {
        return false;
    }
}

/**
 *
 */
void L2A::GLOBAL::Global::ToParameterList(std::shared_ptr<L2A::UTIL::ParameterList>& parameter_list) const
{
    parameter_list->SetOption(ai::UnicodeString("latex_bin_path"), latex_bin_path_);
    parameter_list->SetOption(ai::UnicodeString("latex_engine"), latex_engine_);
    parameter_list->SetOption(ai::UnicodeString("latex_command_options"), latex_command_options_);
    parameter_list->SetOption(ai::UnicodeString("gs_command"), gs_command_);
    parameter_list->SetOption(ai::UnicodeString("warning_boundary_boxes"), warning_boundary_boxes_);
    parameter_list->SetOption(ai::UnicodeString("warning_ai_not_saved"), warning_ai_not_saved_);
}

/**
 *
 */
ai::UnicodeString L2A::GLOBAL::Global::ToString() const
{
    std::shared_ptr<L2A::UTIL::ParameterList> parameter_list = std::make_shared<L2A::UTIL::ParameterList>();
    ToParameterList(parameter_list);
    return parameter_list->ToXMLString(ai::UnicodeString("LaTeX2AI_options"));
}

/**
 *
 */
void L2A::GLOBAL::Global::GetDefaultParameterList(std::shared_ptr<L2A::UTIL::ParameterList>& parameter_list) const
{
    parameter_list->SetOption(ai::UnicodeString("latex_bin_path"), ai::UnicodeString(""));
    parameter_list->SetOption(ai::UnicodeString("latex_engine"), ai::UnicodeString("pdflatex"));
    parameter_list->SetOption(ai::UnicodeString("latex_command_options"),
        ai::UnicodeString("-interaction nonstopmode -halt-on-error -file-line-error"));
    parameter_list->SetOption(ai::UnicodeString("gs_command"), ai::UnicodeString(""));
    parameter_list->SetOption(ai::UnicodeString("warning_boundary_boxes"), true);
    parameter_list->SetOption(ai::UnicodeString("warning_ai_not_saved"), true);
}

/**
 *
 */
bool L2A::GLOBAL::Global::SetFromParameterList(const L2A::UTIL::ParameterList& parameter_list)
{
    bool set_all = true;

    ai::UnicodeString value("latex_bin_path");
    if (parameter_list.OptionExists(value))
        latex_bin_path_ = ai::FilePath(parameter_list.GetStringOption(value));
    else
        set_all = false;

    value = ai::UnicodeString("latex_engine");
    if (parameter_list.OptionExists(value))
        latex_engine_ = parameter_list.GetStringOption(value);
    else
        set_all = false;

    value = ai::UnicodeString("latex_command_options");
    if (parameter_list.OptionExists(value))
        latex_command_options_ = parameter_list.GetStringOption(value);
    else
        set_all = false;

    value = ai::UnicodeString("gs_command");
    if (parameter_list.OptionExists(value))
        gs_command_ = parameter_list.GetStringOption(value);
    else
        set_all = false;

    value = ai::UnicodeString("warning_boundary_boxes");
    if (parameter_list.OptionExists(value))
        warning_boundary_boxes_ = bool(parameter_list.GetIntOption(value));
    else
        set_all = false;

    value = ai::UnicodeString("warning_ai_not_saved");
    if (parameter_list.OptionExists(value))
        warning_ai_not_saved_ = bool(parameter_list.GetIntOption(value));
    else
        set_all = false;

    return set_all;
}

/**
 *
 */
void L2A::GLOBAL::CheckGlobal()
{
    if (_l2a_global == nullptr) l2a_error("The global object is not defined!");
}

/**
 *
 */
const L2A::GLOBAL::Global& L2A::Global() { return L2A::GlobalMutable(); }

/**
 *
 */
L2A::GLOBAL::Global& L2A::GlobalMutable()
{
    L2A::GLOBAL::CheckGlobal();
    return *L2A::GLOBAL::_l2a_global;
}

/**
 *
 */
L2APlugin& L2A::GlobalPluginMutable()
{
    L2A::GLOBAL::CheckGlobal();
    return *L2A::GLOBAL::_l2a_plugin;
}

/**
 *
 */
AppContext L2A::GlobalPluginAppContext() { return AppContext(L2A::GlobalPluginMutable().GetPluginRef()); }
