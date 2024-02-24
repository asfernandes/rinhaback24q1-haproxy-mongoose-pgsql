#!/bin/sh

docker buildx build --progress plain -f src/api/Dockerfile -t asfernandes/rinhaback24q1:haproxy-mongoose-pgsql-api .
