## Introduction
The tool  generates two JSON files in the current working directory: `port_info.json` and `hier_info.json`. The `port_info.json` contains all top modules with their ports which are written as a list of objects. The `hier_info.json` contains the hierarchy information.

## Structure of hier_info.json

- `fileIDs`: list of all source files with unique IDs, which are used in json to avoid repetitive long string names for every module description.

- `modules`: description of project's all modules. Has unique identifier constructed by module name and parameters, this identifier is used as key for modules instances fields. 

- `hierTree`: list of all top level modules (containing filename id, port info, parameters info, module instances with module identifiers pointing to the "modules" section.)

```bash
{
    "fileIDs": {
        "1": "SRC1",
        "2": "SRC2"
    },
    "hierTree": [
        {
            "file": "UNIQUE_FILE_ID",
            "internalSignals": [
                {
                    "name": "NAME",
                    "range": {
                        "lsb": LSB_NUM,
                        "msb": MSB_NUM
                    },
                    "type": "SIGNAL_TYPE"
                },
		...
            ],
            "language": "RTL_LANGUAGE_STANDARD",
            "line": LINE_NUM,
            "moduleInsts": [
                {
                    "file": "UNIQUE_FILE_ID",
                    "instName": "INSTANCE_NAME",
                    "line": LINE_NUM,
                    "module": "MODULE_UNIQUE_NAME",
                    "parameters": [
                        {
                            "name": "PARAM_NAME",
                            "value": "PARAM_VALUE"
                        },
			...
                    ]
                },
		...
            ],
            "parameters": [
                {
                    "name": "PARAM_NAME",
                    "value": PARAM_VALUE
                },
		...
            ],
            "ports": [
                {
                    "direction": "PORT_DIRECTION",
                    "name": "PORT_NAME",
                    "range": {
                        "lsb": LSB_NUM,
                        "msb": MSB_NUM
                    },
                    "type": "PORT_SIGNAL_TYPE"
                },
                ...
            ],
            "topModule": "TOP_MODULE_NAME"
        },
	...
    ],
    "modules": {
        "MODULE_UNIQUE_NAME": {
            "file": "UNIQUE_FILE_ID",
            "internalSignals": [
                {
                    "name": "NAME",
                    "range": {
                        "lsb": LSB_NUM,
                        "msb": MSB_NUM
                    },
                    "type": "SIGNAL_TYPE"
                },
		...
            ],
            "language": "RTL_LANGUAGE_STANDARD",
            "line": LINE_NUM,
            "module": "MODULE_NAME",
            "parameters": [
                {
                    "name": "PARAM_NAME",
                    "value": PARAM_VALUE
                },
		...
            ],
            "ports": [
                {
                    "direction": "PORT_DIRECTION",
                    "name": "PORT_NAME",
                    "range": {
                        "lsb": LSB_NUM,
                        "msb": MSB_NUM
                    },
                    "type": "PORT_SIGNAL_TYPE"
                },
                ...
            ],
            "moduleInsts": [
                {
                    "file": "UNIQUE_FILE_ID",
                    "instName": "INSTANCE_NAME",
                    "line": LINE_NUM,
                    "module": "MODULE_UNIQUE_NAME",
                    "parameters": [
                        {
                            "name": "PARAM_NAME",
                            "value": "PARAM_VALUE"
                        },
			...
                    ]
                },
		...
	    ]
        }
    }
}
```

## Usage
```bash
analyze -f <path_to_instruction_file>
```
where instruction file contain vhdl/verilog/systemverilog source files with options. 

The complete list of supported options are the following:
```bash
	{-vlog95|-vlog2k|-sv2005|-sv2009|-sv2012|-sv} [-D<macro>[=<value>]] <verilog-file/files>
	{-vhdl87|-vhdl93|-vhdl2k|-vhdl2008|-vhdl} <vhdl-file/files>
	-work <libname> {-sv|-vhdl|...} <hdl-file/files>
	-L <libname> {-sv|-vhdl|...} <hdl-file/files>
	-vlog-incdir <directory>
	-vlog-libdir <directory>
	-vlog-define <macro>[=<value>]
	-vlog-undef <macro>
	-top <top-module>
```
