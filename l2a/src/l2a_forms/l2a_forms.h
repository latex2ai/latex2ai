// -----------------------------------------------------------------------------
// MIT License
//
// Copyright (c) 2020 Ivo Steinbrecher (https://github.com/stoani89/LaTeX2AI)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
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
 * \brief Create functions that interact with the forms application.
 */


#ifndef L2A_FORMS_H_
#define L2A_FORMS_H_


// Forward declarations.
namespace L2A
{
    namespace UTIL
    {
        class ParameterList;
    }
}  // namespace L2A


namespace L2A
{
    /**
     * \brief Call a form, pass a parameter list to it and reutrn the parameter list generated by the form.
     * @return False if the form was canceled.
     */
    bool Form(const ai::UnicodeString& form_type, const L2A::UTIL::ParameterList& input_parameter_list,
        std::shared_ptr<L2A::UTIL::ParameterList>& return_parameter_list);

    /**
     * \brief Basically this is a wrapper for the sAIUser->MessageAlert function, but can be overwritten during testing.
     */
    void MessageAlert(const ai::UnicodeString& message_string);
}  // namespace L2A

#endif
