This snippet is an extension of Kavir's MSDP Protocol Handler. It builds on top
of what he created to provide GMCP and GO AHEAD support.

It is free to use, change, and/or distribute.

GMCP is an out of band messaging system that sends and receives information in
a JSON format for ease of readability. Clients and servers can send and receive
information that is not displayed in the main output window of a client in order
to provide additional resources to the player. Things that GMCP can provide include
status bars, enemy information, and mapping capabilities.

The format of this snippet follows the technique Kavir used. Every loop of the mud
code (in update.c) will update the set variables and send the GMCP messaging out
for only variables that have changed since last cycle. This means messages will
only be sent out if they have changed. As well, the client is able to send messages
to the server in order to receive updates or change settings.

If you don't have Kavir's Protocol Handler installed, simply follow the instructions in
the INSTALL_GMCP file. If you already have it installed, you can update your
protocol.c and protocol.h files by:

	Search both files for

	/*************** START GMCP ***************/
	/*************** END GMCP ***************/

	Copy all code between these lines. Paste them in the appropriate places based on
	where they are in the files. Read through INSTALL_GMCP.txt and follow the instructions
	where it says states GMCP ADDON.

Any questions can be sent to Greg via the MUD discord.

For more information about GMCP:
https://www.gammon.com.au/gmcp
