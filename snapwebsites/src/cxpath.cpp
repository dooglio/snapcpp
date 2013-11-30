/*
 * Text:
 *      cxpath.cpp
 *
 * Description:
 *      Compile an XPath to binary byte code.
 *
 * License:
 *      Copyright (c) 2013 Made to Order Software Corp.
 * 
 *      http://snapwebsites.org/
 *      contact@m2osw.com
 * 
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <advgetopt.h>
#include <not_reached.h>
#include <qdomxpath.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <QFile>
#pragma GCC diagnostic pop




const advgetopt::getopt::option cxpath_options[] =
{
    {
        '\0',
        advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
        NULL,
        NULL,
        "Usage: cxpath --<command> [--<opt>] ['<xpath>'] [<filename>.xml] ...",
        advgetopt::getopt::help_argument
    },
    // COMMANDS
    {
        '\0',
        0,
        NULL,
        NULL,
        "commands:",
        advgetopt::getopt::help_argument
    },
    {
        'c',
        0,
        "compile",
        NULL,
        "compile the specified XPath and save it to a .xpath file and optionally print out the compiled code",
        advgetopt::getopt::no_argument
    },
    {
        'd',
        0,
        "disassemble",
        NULL,
        "disassemble the specified .xpath file (if used with the -c, disassemble as we compile)",
        advgetopt::getopt::no_argument
    },
    {
        'h',
        0,
        "help",
        NULL,
        "display this help screen",
        advgetopt::getopt::no_argument
    },
    {
        'x',
        0,
        "execute",
        NULL,
        "execute an xpath (.xpath file or parsed on the fly XPath) against one or more .xml files",
        advgetopt::getopt::required_argument
    },
    // OPTIONS
    {
        '\0',
        0,
        NULL,
        NULL,
        "options:",
        advgetopt::getopt::help_argument
    },
    {
        'n',
        0,
        "namespace",
        NULL,
        "if specified, the namespaces are taken in account, otherwise the DOM ignores them",
        advgetopt::getopt::no_argument
    },
    {
        'o',
        0,
        "output",
        NULL,
        "name of the output file (the .xpath filename)",
        advgetopt::getopt::required_argument
    },
    {
        'p',
        0,
        "xpath",
        NULL,
        "an XPath",
        advgetopt::getopt::required_argument
    },
    {
        'r',
        0,
        "results",
        NULL,
        "display the results of executing the XPath",
        advgetopt::getopt::no_argument
    },
    {
        'v',
        0,
        "verbose",
        NULL,
        "make the process verbose",
        advgetopt::getopt::no_argument
    },
    {
        '\0',
        0,
        "filename",
        NULL,
        NULL, // hidden argument in --help screen
        advgetopt::getopt::default_multiple_argument
    },
    {
        '\0',
        0,
        NULL,
        NULL,
        NULL,
        advgetopt::getopt::end_of_options
    }
};



advgetopt::getopt * g_opt;
bool                g_verbose;
bool                g_results;


void display_node(int j, QDomNode node)
{
    // unfortunate, but QtDOM does not offer a toString() at the
    // QDomNode level, instead they implemented it at the document
    // level... to make use of it you have to create a new document
    // and import the node in there to generate the output
    if(node.isDocument())
    {
        // documents cannot be imported properly
        //node = node.toDocument().documentElement();
        printf("Result[%d] is the entire document.\n", j);
        return;
    }
    QDomDocument document;
    QDomNode copy(document.importNode(node, true));
    document.appendChild(copy);
    printf("Node[%d] = \"%s\"\n", j, document.toByteArray().data());
}



void cxpath_compile()
{
    if(!g_opt->is_defined("xpath"))
    {
        fprintf(stderr, "error: --xpath not defined, nothing to compile.\n");
        exit(1);
    }

    std::string xpath(g_opt->get_string("xpath"));
    if(g_verbose)
    {
        printf("compiling \"%s\" ... \n", xpath.c_str());
    }

    const bool disassemble(g_opt->is_defined("disassemble"));

    QDomXPath dom_xpath;
    dom_xpath.setXPath(QString::fromUtf8(xpath.c_str()), disassemble);

    if(g_opt->is_defined("output"))
    {
        QDomXPath::program_t program(dom_xpath.getProgram());
        const QDomXPath::instruction_t *inst(program.data());
        std::string filename(g_opt->get_string("output"));
        FILE *f(fopen(filename.c_str(), "w"));
        if(f == NULL)
        {
            fprintf(stderr, "error: cannot open output file \"%s\" for writing.\n", filename.c_str());
            exit(1);
        }
        if(fwrite(inst, program.size(), 1, f) != 1)
        {
            fprintf(stderr, "error: I/O error while writing to output file \"%s\".\n", filename.c_str());
            exit(1);
        }
        fclose(f);

        if(g_verbose)
        {
            printf("saved compiled XPath in \"%s\" ... \n", filename.c_str());
        }
    }
}



void cxpath_execute()
{
    std::string program_filename(g_opt->get_string("execute"));
    FILE *f(fopen(program_filename.c_str(), "r"));
    if(f == NULL)
    {
        fprintf(stderr, "error: could not open program file \"%s\" for reading.\n", program_filename.c_str());
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    const int program_size(ftell(f));
    fseek(f, 0, SEEK_SET);
    QDomXPath::program_t program;
    program.resize(program_size);
    if(fread(&program[0], program_size, 1, f) != 1)
    {
        fprintf(stderr, "error: an I/O error occured while reading the program file \"%s\".\n", program_filename.c_str());
        exit(1);
    }

    const bool keep_namespace(g_opt->is_defined("namespace"));
    const bool disassemble(g_opt->is_defined("disassemble"));

    QDomXPath dom_xpath;
    dom_xpath.setProgram(program, disassemble);

    if(g_verbose)
    {
        printf("Original XPath: %s\n", dom_xpath.getXPath().toUtf8().data());
    }

    const int size(g_opt->size("filename"));
    for(int i(0); i < size; ++i)
    {
        if(g_verbose)
        {
            printf("Processing \"%s\" ... ", g_opt->get_string("filename", i).c_str());
        }
        QFile file(QString::fromUtf8(g_opt->get_string("filename", i).c_str()));
        if(!file.open(QIODevice::ReadOnly))
        {
            fprintf(stderr, "error: could not open XML file \"%s\".", g_opt->get_string("filename", i).c_str());
            return;
        }
        QDomDocument document;
        if(!document.setContent(&file, keep_namespace))
        {
            fprintf(stderr, "error: could not read XML file \"%s\".", g_opt->get_string("filename", i).c_str());
            return;
        }
        QDomXPath::node_vector_t result(dom_xpath.apply(document));

        if(g_results)
        {
            const int max(result.size());
            printf("This XPath returned %d nodes\n", max);
            for(int j(0); j < max; ++j)
            {
                display_node(j, result[j]);
            }
        }
    }
}


void cxpath_disassemble()
{
    std::string program_filename(g_opt->get_string("filename"));
    FILE *f(fopen(program_filename.c_str(), "r"));
    if(f == NULL)
    {
        fprintf(stderr, "error: could not open program file \"%s\" for reading.\n", program_filename.c_str());
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    const int program_size(ftell(f));
    fseek(f, 0, SEEK_SET);
    QDomXPath::program_t program;
    program.resize(program_size);
    if(fread(&program[0], program_size, 1, f) != 1)
    {
        fprintf(stderr, "error: an I/O error occured while reading the program file \"%s\".\n", program_filename.c_str());
        exit(1);
    }

    QDomXPath dom_xpath;
    dom_xpath.setProgram(program, true);

    printf("Original XPath: %s\n", dom_xpath.getXPath().toUtf8().data());

    dom_xpath.disassemble();
}


int main(int argc, char *argv[])
{
    std::vector<std::string> empty_list;
    g_opt = new advgetopt::getopt(argc, argv, cxpath_options, empty_list, NULL);
    if(g_opt->is_defined("help"))
    {
        g_opt->usage(advgetopt::getopt::no_error, "Usage: cxpath [--<opt>] [-p '<xpath>'] | [-x <filename>.xpath <filename>.xml ...]");
        snap::NOTREACHED();
    }
    g_verbose = g_opt->is_defined("verbose");
    g_results = g_opt->is_defined("results");

    if(g_opt->is_defined("compile"))
    {
        cxpath_compile();
    }
    else if(g_opt->is_defined("execute"))
    {
        cxpath_execute();
    }
    else if(g_opt->is_defined("disassemble"))
    {
        cxpath_disassemble();
    }

    return 0;
}


// vim: ts=4 sw=4 et
