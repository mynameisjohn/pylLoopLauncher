import networkx as nx
import abc
import random
import itertools
import math
import time

# Scalar product of two iterables
def dot(A, B):
    return sum(a * b for a, b in itertools.zip_longest(A, B, fillvalue = 0))

# returns normalized iterable (as a list)
def nrm(V):
    mag = math.sqrt(float(dot(V, V)))
    return [v / mag for v in V]

# Generic graph node, has a hashable name
# and is iterable. Calling next as well as entering
# and exiting a context are to be overridden
class Node(abc.ABC):
    def __init__(self, name):
        self.name = name

    def __hash__(self):
        return hash(self.name)

    def __eq__(self, other):
        return self.name == other.name

    def __iter__(self):
        return self

    def __repr__(self):
        return str(self.name)

    @abc.abstractmethod
    def __next__(self):
        return None

    @abc.abstractmethod
    def __enter__(self):
        return self

    @abc.abstractmethod
    def __exit__(self, exc_type, exc_val, exc_tb):
        return False

# A Graph State represents a container of
# iterable leaves that, when zipped, yield 
# the tuple of return values from the zip
class State(Node):
    def __init__(self, name, leaves):
        self.name = name
        self.leaves = leaves

    def __repr__(self):
        return super().__repr__()

    def __next__(self):
        if self.Z is not None:
            return next(self.Z)
        raise StopIteration

    # Entering and exiting the graph context
    # reinitializes the leaf nodes by rezipping
    def __enter__(self):
        for l in self.leaves:
            l.__enter__()
        
        self.Z = zip(*[iter(l) for l in self.leaves])
        return self

    def __exit__(self, *args):
        for l in self.leaves:
            l.__exit__(*args)

        self.Z = None
        return False

# For me a leaf node is an iterable
# directly owned by a state object
# that continuously yields values
class Leaf(Node):
    def __init__(self, name, contents):
        self.name = name
        self.contents = contents
        self.nIt = 0

    def __repr__(self):
        return super().__repr__()

    # Calling next on a leaf yields a pair
    # pair[0] = name, pair[1] = value
    def __next__(self):
        self.nIt += 1
        return (self.name, random.choice(self.contents))

    # Entering and exiting the context
    # resets the iteration count
    def __enter__(self):
        self.nIt = 0
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        #print(self.name, 'played for', self.nIt, 'loops')
        self.nIt = 0
        return False

# The StateGraph object owns the networkx DiGraph
# that contains all of the states and leaves. When
# queried it yields a value from the active state
# that is kept alive using a coroutine
class StateGraph:
    # Construct with the graph object, as well as all objects
    # that you'd consider a state already within the graph
    def __init__(self, graph, initialState, stimFn, initialStimulus):
        # TODO can this handle regular graphs?
        if not isinstance(graph, nx.DiGraph):
            raise ValueError('Error: We need a networkx DiGraph')
        else:
            self.G = graph
        
        # The initial state must be one of the graph's nodes
        if initialState not in self.G.nodes():
            print('Warning: Initial state not in graph, choosing one at random')
            self.activeState = random.choice(self.G.nodes())
        else:
            self.activeState = initialState

        # The stimFn parameter is some callable which
        # takes the StateGraph instance that owns it
        # as a parameter and returns the next state,
        # given the current stimulus of the stategraph
        # This was done to keep the notion of what a "stimulus"
        # is as abstract as possible
        self.stimulus = initialStimulus
        self._stimFunc = stimFn
        
        self.numStates = len(self.G.nodes())
        self.keepGoing = True

        # This coroutine runs for the lifetime
        # of the state graph and yields a list
        # containing the active set of leaf values
        # Admittedly it's a bit confusing and probably
        # more trouble than it's worth, but I wanted to
        # keep the state context alive within a function
        def genCoro(self):
            # Before looping, compute the next state given current stimulus
            nextState = self._stimFunc(self)
            while self.keepGoing:
                # Set the active state to the next state
                self.activeState = nextState
                # Enter the active state context
                with self.activeState as activeState:
                    # While the next state is the active state
                    while nextState is activeState:
                        # return the active state's next value
                        # and pause execution until next call
                        yield list(next(activeState))
                        # Compute the next state after the yield above
                        # and break the loop if necessary
                        nextState = self._stimFunc(self)

        # Create the generating coroutine
        # (no need to prime... yet)
        self._genCoro = genCoro(self)

    # You should encapsulate the stimulus logic inside
    # a component passed in on construction that handles
    # choosing the next state given the current stimulus 
    # (set here)
    def SetStimulus(self, stimulus):
        print(stimulus)
        self.stimulus = stimulus

    # This function iterates the coroutine
    # and returns the next value. It also
    # takes in some external stimulus and 
    # determines what state comes next
    def GetNextState(self):
        # Pump the generating coroutine
        # and get the next value
        print('Current State', self.activeState)
        print('stimulus', self.stimulus)
        print('Next State', self._stimFunc(self))
        ret = next(self._genCoro)
        print('Contents', ret)
        print('\n')

        return ret

    # Return a map of leaf names to
    # a list(set(all their values))
    def GetValueMap(self):
        ret = {}
        for n in self.G.nodes_iter():
            for l in n.leaves:
                if l.name not in ret.keys():
                    ret[l.name] = l.contents
                else:
                    ret[l.name] = list(set(ret[l.name]) | set(l.contents))
        return ret