#!/bin/bash

osascript -e 'tell application "Terminal" to do script "docker exec -it sta_main /bin/bash"'
osascript -e 'tell application "Terminal" to do script "docker exec -it satellitea /bin/bash"'