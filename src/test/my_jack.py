import jack
import numpy
import test


jack.attach("python")

jack.register_port("in_1", jack.IsInput)
jack.register_port("out_1", jack.IsOutput)
jack.activate()

#t = test.Test()

#jack.connect("python:out_1", "SL_test_engine:loop_0_in_1")

jack.connect("python:out_1", "python:in_1")

N = jack.get_buffer_size()
Sr = float(jack.get_sample_rate())

no_of_buffers = 4;
test_input = numpy.array([[1.0] * N * no_of_buffers], 'f')
test_output = numpy.zeros((1,N * no_of_buffers), 'f')


i = 0
while i < test_output.shape[1] - N:
    try:
        jack.process(test_input[:,i:i+N], test_output[:,i:i+N])
        i += N
    except jack.InputSyncError:
        pass
    except jack.OutputSyncError:
        pass

jack.deactivate()
jack.detach()

