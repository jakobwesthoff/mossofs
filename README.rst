=======
MossoFS
=======

Scope
=====

This document provides general information about the MossoFS FUSE module. It
presents information about the general idea behind MossoFS. Furthermore
application and usage examples are given.

This document is not intended to provide any kind of documentation of the
inner workings or design principles used during the development of this
filesystem.

What is MossoFS?
================

MossoFS is a FUSE_ based filesystem which allows easy access to `Cloud Files`_
a service provided by mosso_.

What is FUSE?
-------------

FUSE_ stands for "Filesystem in USErspace". It is a linux kernel filesystem
driver, which acts as some kind of proxy between an arbitrary userspace
program, using the fuse library, and the linux kernel filesystem architecture.
Therefore it allows the development of filesystems which are running
completely in userspace instead of kernelspace. 

.. image:: https://raw.github.com/jakobwesthoff/mossofs/master/doc/FUSE_structure.png
   :alt: Structure of the FUSE module.
   :align: center

The picture above has been derived from FUSE_Structure.svg__ found on the
wikimedia commons. It has originally been created by `User:Sven`__ and is
licensed under `CC-by-sa 3.0`__.

__ http://en.wikipedia.org/wiki/File:FUSE_structure.svg
__ http://commons.wikimedia.org/wiki/User:Sven
__ http://creativecommons.org/licenses/by-sa/3.0/

Who is mosso?
-------------

Mosso_ is a service belonging to the well established Rackspace_ company. It
provides different kinds of cloud computing resources. One of them is `Cloud
Files`_, which is the service MossoFS has been written for.

What is Cloud Files?
--------------------

`Cloud Files`_ is a service provided by mosso_, which allows the user to store
arbitrary data in a cloud. The user pays for the amount of data stored as well
as the amount of traffic used.

Rackspace_ describes its service as follows::

    Cloud Files is reliable, scalable and affordable web-based storage for
    backing up and archiving all your static content.

`Kore Nordmann`_ published an interesting `blog post`__ on howto backup svn
repositories on a as needed basis to the mosso cloud. I am using a slightly
modified version of his scripts by myself and it works like a charm.

__ http://kore-nordmann.de/blog/0086_backup_svn_to_mosso_cloud_file.html

Mosso provides a comprehensive REST_ based api to interact with the service, to
store, manage and retrieve files.

To ease the use of data retrieval once it has been stored in the cloud I
started the development of MossoFS. It is a simple FUSE_ based filesystem
application which maps the contents and structure of your `Cloud Files`_
storage to a linux filesystem in a fully transparent way.

Supported Features
==================

MossoFS is a readonly filesystem at the moment. It supports reading of your
`Cloud Files`_ storage. Write-support is planned to be added in a future
release.

Currently supported features
----------------------------

- Support of "Containers"
- Support of virtual directories as described in the `Cloud Files documentation`__
- Full read support of stored files.
- Rudimental caching of retrieved metadata and container listings

__ https://api.mosso.com/guides/cloudfiles/cf-devguide-20090311.pdf

Planned features for future releases
------------------------------------

- Write support for files
- Creation/Deletion support for containers and virtual directories
- Support for extended metadata to store and retrieve informations like
  fileowner and filegroup
- Inteligent caching of retrieved files

Known Limitations
-----------------

Currently read operations on files are not cached at all. The exact amount of
data requested by each read syscall is retrieved from the cloud and returned
to the calling function. This implies the overhead of a full HTTP request
everytime a chunk of data is read. If these chunks are quite small the
performance impact might be huge. 

This development release does not include proper caching strategies for file
reading. Future releases are supposed to have this feature implemented.

Install from source
===================

Needed prerequisites
--------------------

- FUSE library version 26 or later
- cURL library version 7.19 or later
- CMake version 2.6 or later
- gcc C compiler

Steps to compile and install
----------------------------

Download the source code of the current release::
	
	wget http://westhoffswelt.de/data/projects/mossofs/mossofs-latest.tar.gz

Decompress the archive::

	tar xvzf mossofs-latest.tar.gz

Create a build directory and run cmake to configure::

	cd mossofs-*
	mkdir build
	cd build
	cmake ../

Compile the source and link the executable::

	make

Install the executable to */usr/local/bin/mossofs*::

	make install

After finishing this easy steps you can call the mossofs executable by
issueing *mossofs* in your terminal.

Usage Example
=============

The usage of MossoFS is quite intuitive. The *mossofs* executable is called
given your Mosso Files username followed by an @ symbol followed by the
api key provided by Mosso as the first parameter and the path to mount the
filesystem to as second parameter. ::

	mossofs jakob@123456789abcdef /mnt/mosso

After the command is issued a connection to `Cloud Files`_ is established and
all connection parameters are negotiated. The container and file structure is
then mapped to the provided directory. The base structure of this directory
mapping might look something like this::

	.
	|-- container1
	|   `-- some_file.txt
	`-- container2
	    |-- virtual_directory1
		|   `-- another_file.txt
		`-- and_yet_another_file.txt

You may interact with the files in this filesystem like you would with any
other file on your computer. Currently the filesystem does only support read
operations. For future releases write support is planned.


.. _FUSE: http://fuse.sourceforge.net
.. _mosso: http://www.mosso.com
.. _Rackspace: http://www.rackspace.com/index.php
.. _`Cloud Files`: http://www.mosso.com/cloudfiles.jsp
.. _`Kore Nordmann`: http://kore-nordmann.de
.. _REST: http://en.wikipedia.org/wiki/REST


