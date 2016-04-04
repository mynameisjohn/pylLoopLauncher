from GraphClasses import *
from pylLoopLauncher import LoopLauncher, Track
import pylSFMLKeys
from pylSFMLKeys import IsKeyDown
from pylSFMLTime import SFMLTime

def MakeGraph():
    # Construct the directed graph
    G = nx.DiGraph()

    # A state is just drums, bass
    lA1 = Leaf('bass', ['bass.wav'])
    lA2 = Leaf('drums', ['drums.wav']) 
    A = State('A', [lA1, lA2])

    # B state is drum2, bass, sustain, piano
    lB1 = Leaf('bass', ['bass.wav'])
    lB2 = Leaf('drums', ['drums2.wav'])
    lB3 = Leaf('sustain', ['sustain.wav'])
    lB4 = Leaf('piano', ['piano.wav'])
    B = State('B', [lB1, lB2, lB3, lB4])

    # C state is drum2, chords, piano, lead
    lC1 = Leaf('bass', ['bass.wav'])
    lC2 = Leaf('drums', ['drums2.wav'])
    lC3 = Leaf('sustain', ['chords.wav'])
    lC4 = Leaf('piano', ['piano.wav'])
    lC5 = Leaf('lead', ['lead.wav'])
    C = State('C', [lC1, lC2, lC3, lC4, lC5])

    # The first transition state between A and C
    # is drums, bass, guitar chord
    lD1 = Leaf('bass', ['bass.wav'])
    lD2 = Leaf('drums', ['drums.wav'])
    lD3 = Leaf('sustain', ['g_chord.wav'])
    D = State('D', [lD1, lD2, lD3])

    # The second is drum2, bass, all chord, piano
    lE1 = Leaf('bass', ['bass.wav'])
    lE2 = Leaf('drums', ['drums2.wav'])
    lE3 = Leaf('sustain', ['chords.wav'])
    lE4 = Leaf('piano', ['piano.wav'])
    E = State('E', [lE1, lE2, lE3, lE4])

    # The transition state between C and A is just the
    # A state but with the lead playing
    lF1= Leaf('bass', ['bass.wav'])
    lF2 = Leaf('drums', ['drums.wav'])
    lF3 = Leaf('lead', ['lead.wav'])
    F = State('F', [lF1, lF2, lF3])

    # vectors
    toA = [1, 0, 0]
    toB = [0, 1, 0]
    toC = [0, 0, 1]

    # A, B, and C states are self connecting
    G.add_edges_from([(A, A), (B, B), (C, C)])
    G[A][A]['vec'] = toA
    G[B][B]['vec'] = toB
    G[C][C]['vec'] = toC

    # A connects to B connects to C
    G.add_path((A, B, C))
    G[A][B]['vec'] = toB
    G[B][C]['vec'] = toC
        
    # and vice versa
    G.add_path((C, B, A))
    G[C][B]['vec'] = toB
    G[B][A]['vec'] = toA
        
    # A connects to C via D and E
    G.add_path((A, D, E, C))
    G[A][D]['vec'] = toC
    G[D][E]['vec'] = toC
    G[E][C]['vec'] = toC

    # C connects to A via F
    G.add_path((C, F, A))
    G[C][F]['vec'] = toA
    G[F][A]['vec'] = toA

    # D and E and can always come back to A
    G.add_edges_from([(D, A), (E, A)])
    G[D][A]['vec'] = toA
    G[E][A]['vec'] = toA

    # F can come back to C
    G.add_edge(F, C)
    G[F][C]['vec'] = toC

    # D, E, and F can go to B
    G.add_edges_from(((D, B), (E, B), (F, B)))
    G[D][B]['vec'] = toB
    G[E][B]['vec'] = toB
    G[F][B]['vec'] = toB

    # The stim func for this graph
    # treats the stimulus as some vector
    # between [0, 0, 0]  and [1, 1, 1]
    # and finds the neighbor of the current
    # active state with the most suitable connecting edge
    def stimFn(sg):
        # None means repeat
        if sg.stimulus is None:
            return sg.activeState
        # Pick the neighbor state whose 'vec' attribute
        # is most in line with the current stimulus
        nextState = max(sg.G.out_edges_iter(sg.activeState, data = True), key = lambda x : dot(sg.stimulus, x[2]['vec']))[1]
        return nextState

    return StateGraph(G, A, stimFn, None)

g_StateGraph = None
def Initialize(pLoopLauncher):
    global g_StateGraph
    g_StateGraph = MakeGraph()
    trackMap = g_StateGraph.GetValueMap()

    ll = LoopLauncher(pLoopLauncher)
    ll.Initialize(trackMap)

    nextClips = list(c[1] for c in g_StateGraph.GetNextState())
    ll.UpdatePendingClips(nextClips)

    ll.Play()

    return 

def HandleKeys():
    if IsKeyDown(pylSFMLKeys.A):
        return [1, 0, 0]

    if IsKeyDown(pylSFMLKeys.B):
        return [0, 1, 0]

    if IsKeyDown(pylSFMLKeys.C):
        return [0, 0, 1]

def Update(pLoopLauncher):
    if IsKeyDown(pylSFMLKeys.ESC):
        return False

    stimulus = HandleKeys()
    if stimulus is not None:
        g_StateGraph.SetStimulus(stimulus)

    ll = LoopLauncher(pLoopLauncher)
    if ll.NeedsAudio():
        nextClips = list(c[1] for c in g_StateGraph.GetNextState())
        ll.UpdatePendingClips(nextClips)

    return True