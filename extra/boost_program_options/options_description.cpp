// Copyright Vladimir Prus 2002-2004.
// Copyright Bertolt Mildner 2004.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)


#define BOOST_PROGRAM_OPTIONS_SOURCE
#include <boost/program_options/config.hpp>
#include <boost/program_options/options_description.hpp>
// FIXME: this is only to get multiple_occureces class
// should move that to a separate headers.
#include <boost/program_options/parsers.hpp>


#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/throw_exception.hpp>

#include <cassert>
#include <climits>
#include <cstring>
#include <cstdarg>
#include <sstream>
using namespace std;

namespace boost { namespace program_options {

    option_description::option_description()
    {
    }
    
    option_description::
    option_description(const char* name,
                       const value_semantic* s)
    : m_value_semantic(s)
    {
        this->set_name(name);
    }
                                           

    option_description::
    option_description(const char* name,
                       const value_semantic* s,
                       const char* description)
    : m_description(description), m_value_semantic(s)
    {
        this->set_name(name);
    }

    option_description::~option_description()
    {
    }

    bool 
    option_description::match(const std::string& option, bool approx) const
    {
        bool result = false;
        if (!m_long_name.empty()) {

            if (*m_long_name.rbegin() == '*')
            {
                // The name ends with '*'. Any specified name with the given
                // prefix is OK.
                if (option.find(m_long_name.substr(0, m_long_name.length()-1))
                    == 0)
                    result = true;
            }

            if (approx)
            {
                if (m_long_name.find(option) == 0)
                    result = true;
            }
            else
            {
                if (m_long_name == option)
                    result = true;
            }
        }
         
        if (m_short_name == option)
            result = true;

        return result;        
    }

    const std::string& 
    option_description::key(const std::string& option) const
    {        
        if (!m_long_name.empty()) 
            if (m_long_name.find('*') != string::npos)
                // The '*' character means we're long_name
                // matches only part of the input. So, returning
                // long name will remove some of the information,
                // and we have to return the option as specified
                // in the source.
                return option;
            else
                return m_long_name;
        else
            return m_short_name;
    }

    const std::string&
    option_description::long_name() const
    {
        return m_long_name;
    }

    option_description&
    option_description::set_name(const char* _name)
    {
        std::string name(_name);
        string::size_type n = name.find(',');
        if (n != string::npos) {
            assert(n == name.size()-2);
            m_long_name = name.substr(0, n);
            m_short_name = '-' + name.substr(n+1,1);
        } else {
            m_long_name = name;
        }
        return *this;
    }

    const std::string&
    option_description::description() const
    {
        return m_description;
    }

    shared_ptr<const value_semantic>
    option_description::semantic() const
    {
        return m_value_semantic;
    }
    
    std::string 
    option_description::format_name() const
    {
        if (!m_short_name.empty())
            return string(m_short_name).append(" [ --").
            append(m_long_name).append(" ]");
        else
            return string("--").append(m_long_name);
    }

    std::string 
    option_description::format_parameter() const
    {
        if (m_value_semantic->max_tokens() != 0)
            return m_value_semantic->name();
        else
            return "";
    }

    options_description_easy_init::
    options_description_easy_init(options_description* owner)
    : owner(owner)
    {}

    options_description_easy_init&
    options_description_easy_init::
    operator()(const char* name,
               const char* description)
    {
        // Create untypes semantic which accepts zero tokens: i.e. 
        // no value can be specified on command line.
        // FIXME: does not look exception-safe
        shared_ptr<option_description> d(
            new option_description(name, new untyped_value(true), description));

        owner->add(d);
        return *this;
    }

    options_description_easy_init&
    options_description_easy_init::
    operator()(const char* name,
               const value_semantic* s)
    {
        shared_ptr<option_description> d(new option_description(name, s));
        owner->add(d);
        return *this;
    }

    options_description_easy_init&
    options_description_easy_init::
    operator()(const char* name,
               const value_semantic* s,
               const char* description)
    {
        shared_ptr<option_description> d(new option_description(name, s, description));

        owner->add(d);
        return *this;
    }

    options_description::options_description(unsigned line_length)
    : m_line_length(line_length)
    {}

    options_description::options_description(const string& caption,
                                             unsigned line_length)
    : m_caption(caption), m_line_length(line_length)
    {}

    void
    options_description::add(shared_ptr<option_description> desc)
    {
        m_options.push_back(desc);
        belong_to_group.push_back(false);
    }

    options_description&
    options_description::add(const options_description& desc)
    {
        shared_ptr<options_description> d(new options_description(desc));
        groups.push_back(d);

        for (size_t i = 0; i < desc.m_options.size(); ++i) {
            add(desc.m_options[i]);
            belong_to_group.back() = true;
        }

        return *this;
    }

    options_description_easy_init
    options_description::add_options()
    {       
        return options_description_easy_init(this);
    }

    const option_description&
    options_description::find(const std::string& name, bool approx) const
    {
        const option_description* d = find_nothrow(name, approx);
        if (!d)
            boost::throw_exception(unknown_option(name));
        return *d;
    }

    const std::vector< shared_ptr<option_description> >& 
    options_description::options() const
    {
        return m_options;
    }

    const option_description* 
    options_description::find_nothrow(const std::string& name, 
                                      bool approx) const
    {
        int found = -1;
        // We use linear search because matching specified option
        // name with the declared option name need to take care about
        // case sensitivity and trailing '*' and so we can't use simple map.
        for(unsigned i = 0; i < m_options.size(); ++i)
        {
            if (m_options[i]->match(name, approx))
            {
                if (found != -1)
                {
                    vector<string> alts;
                    // FIXME: the use of 'key' here might not
                    // be the best approach.
                    alts.push_back(m_options[found]->key(name));
                    alts.push_back(m_options[i]->key(name));
                    boost::throw_exception(ambiguous_option(name, alts));
                }
                else
                {
                    found = i;
                }
            }
        }
        if (found != -1) {
            return m_options[found].get();
        } else {
            return 0;
        }
    }

    BOOST_PROGRAM_OPTIONS_DECL
    std::ostream& operator<<(std::ostream& os, const options_description& desc)
    {
        desc.print(os);
        return os;
    }

    namespace {

        void format_paragraph(std::ostream& os,
                              std::string par,
                              unsigned first_column_width,
                              unsigned line_length)
        {                    
            // index of tab (if present) is used as additional indent relative
            // to first_column_width if paragrapth is spanned over multiple
            // lines if tab is not on first line it is ignored
            string::size_type par_indent = par.find('\t');

            if (par_indent == string::npos)
            {
                par_indent = 0;
            }
            else
            {
                // only one tab per paragraph allowed
                if (count(par.begin(), par.end(), '\t') > 1)
                {
                    boost::throw_exception(program_options::error(
                        "Only one tab per paragraph is allowed"));
                }
          
                // erase tab from string
                par.erase(par_indent, 1);

                // this assert may fail due to user error or 
                // environment conditions!
                assert(par_indent < (line_length - first_column_width));

                // ignore tab if not on first line
                if (par_indent >= (line_length - first_column_width))
                {
                    par_indent = 0;
                }            
            }
          
            if (par.size() < (line_length - first_column_width))
            {
                os << par;
            }
            else
            {
                string::const_iterator       line_begin = par.begin();
                const string::const_iterator par_end = par.end();

                bool first_line = true; // of current paragraph!
            
                unsigned indent = first_column_width;
            
                while (line_begin < par_end)  // paragraph lines
                {
                    if (!first_line)
                    {
                        // trimm leading single spaces
                        // if (firstchar == ' ') &&
                        //    ((exists(firstchar + 1) && (firstchar + 1 != ' '))
                        if ((*line_begin == ' ') &&
                            ((line_begin + 1 < par_end) &&
                             (*(line_begin + 1) != ' ')))
                        {
                            line_begin += 1;  // line_begin != line_end
                        }
                    }

                    string::const_iterator line_end;
                
                    if (line_begin + (line_length - indent) > par_end)
                    {
                        line_end = par_end;
                    }
                    else
                    {
                        line_end = line_begin + (line_length - indent);
                    }
            
                    // prevent chopped words
                    // if (lastchar != ' ') &&
                    //    ((exists(lastchar + 1) && (lastchar + 1 != ' '))
                    if ((*(line_end - 1) != ' ') &&
                        ((line_end < par_end) && (*line_end != ' ')))
                    {
                        // find last ' ' in the second half of the current paragraph line
                        string::const_iterator last_space =
                            find(reverse_iterator<string::const_iterator>(line_end - 1),
                                 reverse_iterator<string::const_iterator>(line_begin - 1),
                                 ' ')
                            .base();
                
                        if (last_space != line_begin - 1)
                        {                 
                            // is last_space within the second half ot the 
                            // current line
                            if (unsigned(distance(last_space, line_end)) < 
                                (line_length - indent) / 2)
                            {
                                line_end = last_space;
                            }
                        }                                                
                    } // prevent chopped words
             
                    // write line to stream
                    copy(line_begin, line_end, ostream_iterator<char>(os));
              
                    if (first_line)
                    {
                        indent = first_column_width + par_indent;
                        first_line = false;
                    }

                    // more lines to follow?
                    if (line_end != par_end)
                    {
                        os << '\n';
                
                        for(unsigned pad = indent; pad > 0; --pad)
                        {
                            os.put(' ');
                        }                                                        
                    }
              
                    // next line starts after of this line
                    line_begin = line_end;              
                } // paragraph lines
            }          
        }                              
        
        void format_description(std::ostream& os,
                                const std::string& desc, 
                                unsigned first_column_width,
                                unsigned line_length)
        {
            // we need to use one char less per line to work correctly if actual
            // console has longer lines
            assert(line_length > 1);
            if (line_length > 1)
            {
                --line_length;
            }

            // line_length must be larger than first_column_width
            // this assert may fail due to user error or environment conditions!
            assert(line_length > first_column_width);

            // Note: can't use 'tokenizer' as name of typedef -- borland
            // will consider subsequence uses of 'tokenizer' as uses of
            // boost::tokenizer, not typedef.
            typedef boost::tokenizer<boost::char_separator<char> > tok;
          
            tok paragraphs(
                desc,
                char_separator<char>("\n", "", boost::keep_empty_tokens));
          
            tok::const_iterator       par_iter = paragraphs.begin();                
            const tok::const_iterator par_end = paragraphs.end();

            while (par_iter != par_end)  // paragraphs
            {
                format_paragraph(os, *par_iter, first_column_width, 
                                 line_length);
            
                ++par_iter;
            
                // prepair next line if any
                if (par_iter != par_end)
                {
                    os << '\n';
              
                    for(unsigned pad = first_column_width; pad > 0; --pad)
                    {
                        os.put(' ');
                    }                    
                }            
            }  // paragraphs
        }
    
        void format_one(std::ostream& os, const option_description& opt, 
                        unsigned first_column_width, unsigned line_length)
        {
            stringstream ss;
            ss << "  " << opt.format_name() << ' ' << opt.format_parameter();
            
            // Don't use ss.rdbuf() since g++ 2.96 is buggy on it.
            os << ss.str();

            if (!opt.description().empty())
            {
                for(unsigned pad = first_column_width - ss.str().size(); 
                    pad > 0; 
                    --pad)
                {
                    os.put(' ');
                }
            
                format_description(os, opt.description(),
                                   first_column_width, line_length);
            }
        }
    }

    void 
    options_description::print(std::ostream& os) const
    {
        if (!m_caption.empty())
            os << m_caption << ":\n";

        /* Find the maximum width of the option column */
        unsigned width(23);
        unsigned i; // vc6 has broken for loop scoping
        for (i = 0; i < m_options.size(); ++i)
        {
            const option_description& opt = *m_options[i];
            stringstream ss;
            ss << "  " << opt.format_name() << ' ' << opt.format_parameter();
            width = (max)(width, static_cast<unsigned>(ss.str().size()));            
        }
        
        /* add an additional space to improve readability */
        ++width;
            
        /* The options formatting style is stolen from Subversion. */
        for (i = 0; i < m_options.size(); ++i)
        {
            if (belong_to_group[i])
                continue;

            const option_description& opt = *m_options[i];

            format_one(os, opt, width, m_line_length);

            os << "\n";
        }

        for (unsigned j = 0; j < groups.size(); ++j) {            
            os << "\n" << *groups[j];
        }
    }

}}
