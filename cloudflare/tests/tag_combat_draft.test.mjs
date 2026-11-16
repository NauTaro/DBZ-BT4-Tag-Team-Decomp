import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {test} from 'node:test';

const wasm = await WebAssembly.instantiate(
    readFileSync(new URL('../kernel.wasm', import.meta.url)), {});
const k = wasm.instance.exports;
const input = new DataView(k.memory.buffer, k.Kernel_Input(), 512);
const output = new DataView(k.memory.buffer, k.Kernel_Output(), 512);
const write = (offset, value) => input.setUint32(offset, value, true);
function request(command, a = 0, b = 0, seat = -1) {
    new Uint8Array(input.buffer, input.byteOffset, input.byteLength).fill(0);
    for (const [offset, value] of [[0,0x54540009],[4,command],[8,k.Kernel_Epoch()],
        [12,a],[16,b],[20,seat]]) write(offset,value);
}

test('private combat rules carry boss settings and let teammates draft independently', () => {
    k.Kernel_Reset();
    request(1, 2, 0, 0);
    let offset = 72;
    const rules = [2,1,0,1,1,150,0,0,10,0,25,40,1,30];
    for (let i=0; i<10; ++i) rules.push(1,0,0,100,0);
    for (let i=0; i<10; ++i) rules.push(i===0 ? (1<<18)|150|(50<<9) : 100|(100<<9));
    for (const word of rules) { write(offset,word); offset+=4; }
    assert.equal(k.Kernel_Command(101),0);
    request(2);
    write(68,2);
    assert.equal(k.Kernel_Command(102),9);
    for (const user of [102,103,104]) {
        request(2);
        write(68,3);
        assert.equal(k.Kernel_Command(user),0);
    }
    request(3);
    assert.equal(k.Kernel_Command(101),0);
    assert.equal(k.Kernel_Seat(103),2);
    request(4,32,0,2);
    assert.equal(k.Kernel_Command(103),0);
    request(4,33,0,0);
    assert.equal(k.Kernel_Command(103),4);
    request(4,0,0,0);
    assert.equal(k.Kernel_Command(101),0);
    request(12);
    assert.equal(k.Kernel_Command(103),4);
    assert.equal(k.Kernel_Phase(),2);
    const epoch=k.Kernel_Epoch();
    request(4,-1,0,2);
    assert.equal(k.Kernel_Command(103),0);
    assert.equal(k.Kernel_Epoch(),epoch);
    assert.equal(k.Kernel_Phase(),2);
    request(4,35,0,2);
    assert.equal(k.Kernel_Command(103),0);
    k.Kernel_Snapshot(103,0,k.Kernel_Input());
    assert.equal(output.getUint32(96,true),0);
    assert.equal(output.getUint32(176,true),35);
    k.Kernel_OptionsSnapshot();
    assert.equal(output.getUint32(280,true),(1<<18)|150|(50<<9));
});

test('Back withdraws host CPU and guest choices without cancelling either draft format', () => {
    for (const expanded of [false,true]) {
        k.Kernel_Reset();request(1,2,0,0);write(52,1);
        if (expanded) {
            const values=[2,1,0,1,1,150,0,0,10,0,25,40,1,30];
            for(let i=0;i<10;++i)values.push(1,0,0,100,0);
            for(let i=0;i<10;++i)values.push(100|(100<<9));
            values.forEach((v,i)=>write(72+i*4,v));
        }
        assert.equal(k.Kernel_Command(101),0);
        request(2);write(68,3);assert.equal(k.Kernel_Command(102),0);
        const guest=k.Kernel_Seat(102);assert.equal(guest,1);
        request(3);assert.equal(k.Kernel_Command(101),0);
        for(const [user,seat,chara] of [[101,0,0],[102,1,13],[101,2,35]]) {
            request(4,chara,0,seat);assert.equal(k.Kernel_Command(user),0);
        }
        const epoch=k.Kernel_Epoch();
        request(4,-1,0,2);assert.equal(k.Kernel_Command(102),4);
        request(4,-1,0,2);assert.equal(k.Kernel_Command(101),0);
        assert.equal(k.Kernel_Phase(),2);assert.equal(k.Kernel_Epoch(),epoch);
        k.Kernel_Snapshot(101,0,k.Kernel_Input());
        assert.equal(output.getInt32(96,true),0);
        assert.equal(output.getInt32(136,true),13);
        assert.equal(output.getInt32(176,true),-1);
        request(4,-1,0,1);assert.equal(k.Kernel_Command(101),4);
        request(4,-1,0,1);assert.equal(k.Kernel_Command(102),0);
        assert.equal(k.Kernel_Phase(),2);assert.equal(k.Kernel_Epoch(),epoch);
    }
});
