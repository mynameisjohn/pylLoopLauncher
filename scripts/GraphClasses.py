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

#import random
#import itertools
#import networkx

#def randomGen(container):
#    while True:
#        yield random.choice(container)

#class Leaf:
#    def __init__(self, name, clipList, genFunc = itertools.cycle):
#        self.name = name
#        self.clipList = clipList
#        self.genFunc = genFunc

#    def __iter__(self):
#        def genFunc(self):
#            yield from self.genFunc(self.clipList)
#        return genFunc(self)

#    def __hash__(self):
#        return hash(self.name)

#    def __eq__(self, other):
#        return self.name == other.name
    
#    def __str__(self):
#        return 'Node ' + self.name

#    def __repr__(self):
#        return str(self)

#class Node:
#    def __init__(self, name, parentGraph, leaves):
#        self.name = name
#        self.parentGraph = parentGraph
#        self.leafGraph = networkx.DiGraph()
#        self.leafGraph.add_node(self)
#        self.leafGraph.add_edges_from([(self, l) for l in leaves])

#    def __iter__(self):
#        def genFunc(self):
#            leafGens = [iter(s) for s in self.leafGraph.successors_iter(self)]
#            Z = zip(*leafGens)
#            yield from Z
#        return genFunc(self)

#    def __hash__(self):
#        return hash(self.name)

#    def __eq__(self, other):
#        return self.name == other.name

#    def __str__(self):
#        return 'Node ' + self.name

#    def __repr__(self):
#        return str(self)

#class Edge:
#    def __init__(self, parentGraph, node):
#        self.parentGraph = parentGraph
#        self.targetNode = node

#    def __iter__(self):
#        def genFunc(self):
#            yield
#            nodeIt = iter(self.targetNode)
#            while True:
#                nextList = next(nodeIt)
#                val = yield nextList
#        return genFunc(self)

#class TransitionEdge:
#    def __init__(self, parentGraph, numIt, targetNode, nextNode):
#        self.parentGraph = parentGraph
#        self.numIt = numIt
#        self.targetNode = targetNode
#        self.nextNode = nextNode

#    def __iter__(self):
#        def genFunc(self):
#            yield
#            nodeIt = iter(self.targetNode)
#            for i in range(self.numIt):
#                nextList = next(nodeIt)
#                val = yield nextList
#            nextEdge = self.parentGraph[self.targetNode][self.nextNode]
#            nextIt = iter(nextEdge['object'])
#            next(nextIt)
#            return (nextIt, nextEdge, self.nextNode)
#        return genFunc(self)

#class StateManager:
#    def __init__(self):
#        G = networkx.DiGraph()
        
#        # A state is just drums, bass
#        lA1 = Leaf('bass', ['bass.wav'])
#        lA2 = Leaf('drums', ['drums.wav'])
#        nodeA = Node('A', G, [lA1, lA2])

#        # B state is drum2, bass, sustain, piano
#        lB1 = Leaf('bass', ['bass.wav'])
#        lB2 = Leaf('drums', ['drums2.wav'])
#        lB3 = Leaf('sustain', ['sustain.wav'])
#        lB4 = Leaf('piano', ['piano.wav'])
#        nodeB = Node('B', G, [lB1, lB2, lB3, lB4])

#        # C state is drum2, chords, piano, lead
#        lC1 = Leaf('bass', ['bass.wav'])
#        lC2 = Leaf('drums', ['drums2.wav'])
#        lC3 = Leaf('sustain', ['chords.wav'])
#        lC4 = Leaf('piano', ['piano.wav'])
#        lC5 = Leaf('lead', ['lead.wav'])
#        nodeC = Node('C', G, [lC1, lC2, lC3, lC4, lC5])

#        # The first transition state between A and C
#        # is drums, bass, guitar chord
#        lAC11 = Leaf('bass', ['bass.wav'])
#        lAC12 = Leaf('drums', ['drums.wav'])
#        lAC13 = Leaf('sustain', ['g_chord.wav'])
#        nodeAC1 = Node('AC1', G, [lAC11, lAC12, lAC13])

#        # The second is drum2, bass, all chord, piano
#        lAC21 = Leaf('bass', ['bass.wav'])
#        lAC22 = Leaf('drums', ['drums2.wav'])
#        lAC23 = Leaf('sustain', ['chords.wav'])
#        lAC24 = Leaf('piano', ['piano.wav'])
#        nodeAC2 = Node('AC2', G, [lAC21, lAC22, lAC23, lAC24])

#        # The transition state between C and A is just the
#        # A state but with the lead playing
#        lCA11 = Leaf('bass', ['bass.wav'])
#        lCA12 = Leaf('drums', ['drums.wav'])
#        lCA13 = Leaf('lead', ['lead.wav'])
#        nodeCA1 = Node('CA1', G, [lCA11, lCA12, lCA13])

#        # Connect A and B, both ways
#        G.add_edge(nodeA, nodeB, object = Edge(G, nodeB), weight = 3)
#        G.add_edge(nodeB, nodeA, object = Edge(G, nodeA), weight = 3)

#        # Connect B and C, both ways
#        G.add_edge(nodeB, nodeC, object = Edge(G, nodeC), weight = 3)
#        G.add_edge(nodeC, nodeB, object = Edge(G, nodeB), weight = 3)

#        # Connect A and C using the transition states
#        G.add_edge(nodeA, nodeAC1, object = TransitionEdge(G, 1, nodeAC1, nodeAC2), weight = 1)
#        G.add_edge(nodeAC1, nodeAC2, object = TransitionEdge(G, 1, nodeAC2, nodeC), weight = 1)
#        G.add_edge(nodeAC2, nodeC, object = Edge(G, nodeC), weight = 1)

#        # Connect C and A using a transition state
#        G.add_edge(nodeC, nodeCA1, object = TransitionEdge(G, 1, nodeCA1, nodeA), weight = 1)
#        G.add_edge(nodeCA1, nodeA, object = Edge(G, nodeA), weight = 1)

#        # Create the entry state that we use to 'lead in' to the graph
#        G.add_edge('entry', nodeA, object = Edge(G, nodeA), weight = 1)
#        self.G = G
#        self.activeNode = nodeA
#        self.activeEdge = self.G['entry'][nodeA]
#        self.edgeCoro = iter(self.activeEdge['object'])
#        next(self.edgeCoro)

#    def GetTrackMap(self):
#        ret = {}
#        for n in self.G.nodes_iter():
#            if isinstance(n, Node):
#                for l in n.leafGraph.successors_iter(n):
#                    if l.name in ret.keys():
#                        ret[l.name] = list(set(l.clipList) | set(ret[l.name]))
#                    else:
#                        ret[l.name] = l.clipList
#        return ret

#    def GetNextClips(self):
#        try:
#            nextClip = next(self.edgeCoro)
#        except StopIteration as exc:
#            print('switching generator from', self.activeNode.name, 'to', exc.value[2].name)
#            self.edgeCoro = exc.value[0]
#            self.activeEdge = exc.value[1]
#            self.activeNode = exc.value[2]
#            nextClip = next(self.edgeCoro)
#        return nextClip

#    def GetNode(self, nodeName):
#        return Node(nodeName, self.G, [])

#    def GetActiveState(self):
#        return self.activeNode

#    def SetActiveState(self, state):
#        if self.activeNode.name != state and isinstance(self.activeEdge['object'], Edge):
#            path = networkx.dijkstra_path(self.G, self.activeNode, self.GetNode(state))
#            if len(path) > 1:
#                print('changing from', self.activeNode.name, 'to', state, 'via', path)
#                self.activeEdge = self.G[self.activeNode][path[1]]
#                self.activeNode = path[1]
#                self.edgeCoro = iter(self.activeEdge['object'])
#                next(self.edgeCoro)

#if __name__ == '__main__':
#    S = StateManager()
#    for i in range(3):
#        print(S.GetNextClips())
#    S.SetActiveState('B')
#    for i in range(3):
#        print(S.GetNextClips())
#    S.SetActiveState('C')
#    for i in range(3):
#        print(S.GetNextClips())
#    S.SetActiveState('A')
#    for i in range(3):
#        print(S.GetNextClips())
#    S.SetActiveState('C')
#    for i in range(3):
#        print(S.GetNextClips())