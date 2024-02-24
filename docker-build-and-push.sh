#!/bin/sh
set -e

./docker-build.sh
docker push asfernandes/rinhaback24q1:haproxy-mongoose-pgsql-api
