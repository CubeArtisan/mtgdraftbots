if (typeof(Module) === "undefined") Module = {};
Module["print"] = function(s) { console.log("stdout:", s); };
Module["printErr"] = function(s) { console.error("stderr:", s); };
    