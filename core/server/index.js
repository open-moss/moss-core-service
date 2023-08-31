import jayson from "jayson";

const server = new jayson.Server({
    add: function (args, callback) {
        callback(null, args[0] + args[1]);
    }
});

server.http().listen(3000);