#include "common.h"
#include "socket.h"
#include "game.h"
#include "conf.h"
#include "db.h"

int main()
{
	c.load();

	/*if (db.Connect(3, c.getsql().dbname.c_str(), c.getsql().user.c_str(), c.getsql().secret.c_str())) {
		msglog(INFO, "Connected to database server, odbc %s.", c.getsql().dbname.c_str());
	}
	else {
		msglog(ERROR, "Failed to connect to database server, odbc %s.", c.getsql().dbname.c_str());
		return -1;
	}*/

	le_start();

	return 0;
}