-module(peryl).

%% API exports
-export([demo/0, run/1]).
-on_load(init/0).

%%====================================================================
%% API functions
%%====================================================================
demo() ->
    erlang:nif_error({nif_not_loaded, ?MODULE}).

run(_) ->
    erlang:nif_error({nif_not_loaded, ?MODULE}).

%%====================================================================
%% Internal functions
%%====================================================================
init() ->
    PrvDir = case code:priv_dir(?MODULE) of
                 {error, bad_name} ->
                     Dir = code:which(?MODULE),
                     filename:join([filename:dirname(Dir), "..", "priv"]);
                 Dir -> Dir
             end,
    SoName = filename:join(PrvDir, atom_to_list(?MODULE)),
    erlang:load_nif(SoName, 0).
