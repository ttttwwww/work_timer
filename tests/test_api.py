"""HTTP regression tests against an isolated database; no production data is used.
Run: python3 tests/test_api.py /absolute/path/to/WorkTimer
"""
import concurrent.futures
import json
import pathlib
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.error
import urllib.request

BINARY = str(pathlib.Path(sys.argv.pop(1)).resolve())

class ApiTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name)
        # An original database without the later note column must still work.
        with sqlite3.connect(self.root / 'test.db') as db:
            db.execute('CREATE TABLE sessions(id INTEGER PRIMARY KEY AUTOINCREMENT,start_time INTEGER NOT NULL,end_time INTEGER,type TEXT NOT NULL)')
            db.execute("INSERT INTO sessions VALUES(1,100,200,'formal')")
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            self.port = sock.getsockname()[1]
        (self.root / 'config.json').write_text(json.dumps({'port':self.port,'bind_address':'127.0.0.1','database_path':str(self.root/'test.db')}, indent=2))
        self.start()

    def start(self):
        self.process = subprocess.Popen([BINARY], cwd=self.root, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if self.process.poll() is not None:
                self.fail('Server failed to start')
            try:
                self.request('/api/state')
                return
            except OSError:
                time.sleep(.05)
        self.fail('Server startup timed out')

    def stop(self):
        self.process.terminate()
        try: self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill(); self.process.wait()

    def tearDown(self):
        self.stop()
        self.tmp.cleanup()

    def request(self, path, method='GET', body=None, status=200):
        request=urllib.request.Request(f'http://127.0.0.1:{self.port}{path}',data=json.dumps(body).encode() if body is not None else None,method=method,headers={'Content-Type':'application/json'})
        try: response=urllib.request.urlopen(request,timeout=5)
        except urllib.error.HTTPError as error: response=error
        with response:
            payload=response.read().decode()
            self.assertEqual(response.status,status,(path,payload))
            return json.loads(payload)

    def reset_legacy_board(self, version):
        """Input: old schema version 0/1. Output: empty pre-location task tables.
        Called only with this test's temporary server stopped; sessions survive.
        """
        with sqlite3.connect(self.root/'test.db') as db:
            db.executescript('''
                DROP TABLE todo_progress;
                DROP TABLE node_progress;
                DROP TABLE node_todos;
                DROP TABLE problem_nodes;
                DROP TABLE tasks;
                CREATE TABLE tasks(id INTEGER PRIMARY KEY AUTOINCREMENT,title TEXT NOT NULL,description TEXT NOT NULL DEFAULT '',status TEXT NOT NULL DEFAULT 'todo' CHECK(status IN ('todo','doing','done')),created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL);
                CREATE TABLE problem_nodes(id INTEGER PRIMARY KEY AUTOINCREMENT,task_id INTEGER NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,title TEXT NOT NULL,description TEXT NOT NULL DEFAULT '',status TEXT NOT NULL DEFAULT 'open' CHECK(status IN ('open','doing','resolved')),created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL);
                CREATE TABLE node_todos(id INTEGER PRIMARY KEY AUTOINCREMENT,node_id INTEGER NOT NULL REFERENCES problem_nodes(id) ON DELETE CASCADE,title TEXT NOT NULL,done INTEGER NOT NULL DEFAULT 0 CHECK(done IN (0,1)));
                CREATE TABLE node_progress(id INTEGER PRIMARY KEY AUTOINCREMENT,node_id INTEGER NOT NULL REFERENCES problem_nodes(id) ON DELETE CASCADE,content TEXT NOT NULL,created_at INTEGER NOT NULL);
            ''')
            if version == 1:
                db.executescript('''
                    CREATE TABLE todo_progress(id INTEGER PRIMARY KEY AUTOINCREMENT,todo_id INTEGER NOT NULL REFERENCES node_todos(id) ON DELETE CASCADE,content TEXT NOT NULL,created_at INTEGER NOT NULL);
                    CREATE INDEX progress_todo ON todo_progress(todo_id);
                ''')
            db.execute(f'PRAGMA user_version={version}')

    def test_edit_progress_preserves_identity_and_rejects_invalid_updates(self):
        """Input: saved logs and edits. Output: only the selected body changes and persists."""
        task = self.request('/api/tasks', 'POST', {'title':'任务','description':'','status':'todo'}, 201)['id']
        node = self.request(f'/api/tasks/{task}/nodes', 'POST', {'title':'节点','description':'','status':'open'}, 201)['id']
        todo = self.request(f'/api/nodes/{node}/todos', 'POST', {'title':'待办'}, 201)['id']
        first = self.request(f'/api/todos/{todo}/progress', 'POST', {'content':'旧正文'}, 201)['id']
        self.request(f'/api/todos/{todo}/progress', 'POST', {'content':'另一条记录'}, 201)
        before = self.request('/api/tasks')
        content = '  修正后的记录\n中文、引号 " 与路径 D:\\Notes\\test.md\n<script>plain text</script>  '
        self.request(f'/api/progress/{first}', 'PUT', {'content':content,'todo_id':999,'created_at':0})
        expected = dict(before)
        expected['progress'] = [dict(entry, content=content) if entry['id'] == first else entry for entry in before['progress']]
        self.assertEqual(self.request('/api/tasks'), expected)
        # An identical update must also succeed; blank/invalid edits never erase text.
        self.request(f'/api/progress/{first}', 'PUT', {'content':content})
        for body in ({}, {'content':None}, {'content':42}, {'content':''}, {'content':' \n\t'}, {'content':'中'*6667}):
            self.request(f'/api/progress/{first}', 'PUT', body, 400)
        self.request('/api/progress/0', 'PUT', {'content':'正文'}, 400)
        self.request('/api/progress/999999', 'PUT', {'content':'正文'}, 404)
        self.stop()
        self.start()
        self.assertEqual(self.request('/api/tasks'), expected)
        self.request(f'/api/progress/{first}', 'DELETE')
        self.request(f'/api/progress/{first}', 'PUT', {'content':'已删除的记录'}, 404)

    def test_timer_validation_and_trim(self):
        state=self.request('/api/state')
        self.assertAlmostEqual(state['server_time'],time.time(),delta=2)
        self.assertEqual(state['sessions'][0]['id'],1)
        item=self.request('/api/start','POST',{'type':'formal'},201)
        self.request('/api/start','POST',{'type':'informal'},409)
        self.request('/api/delete','POST',{'id':item['id']},409)
        session=self.request('/api/state')['sessions'][0]
        self.request('/api/stop','POST',{'id':item['id'],'end_time':session['start_time']-1},400)
        self.request('/api/stop','POST',{'id':item['id'],'end_time':int(time.time())+3600},400)
        self.request('/api/stop','POST',{'id':item['id'],'end_time':session['start_time']})
        saved=self.request('/api/history')[0]
        self.assertEqual(saved['end_time'],session['start_time'])
        second=self.request('/api/start','POST',{'type':'informal'},201)
        self.request('/api/stop','POST',{'id':item['id']},409)
        self.assertEqual(self.request('/api/history')[0]['end_time'],0)
        self.request('/api/stop','POST',{'id':second['id']})
        self.request('/api/delete','POST',{'id':item['id']})

    def test_concurrent_start(self):
        def start():
            req=urllib.request.Request(f'http://127.0.0.1:{self.port}/api/start',data=b'{"type":"formal"}',headers={'Content-Type':'application/json'})
            try:
                with urllib.request.urlopen(req) as response: return response.status
            except urllib.error.HTTPError as error:
                error.close(); return error.code
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results=list(pool.map(lambda _:start(),range(8)))
        self.assertEqual(results.count(201),1)
        self.assertEqual(results.count(409),7)
        self.assertEqual(sum(s['end_time']==0 for s in self.request('/api/history')),1)

    def test_notes_and_bad_requests(self):
        note="今天的 O'Brien 测试'); DROP TABLE sessions; --\n第二行"
        self.request('/api/note','POST',{'id':1,'note':note})
        self.assertEqual(self.request('/api/history')[0]['note'],note)
        for body in ({}, {'type':1}, {'type':'unknown'}): self.request('/api/start','POST',body,400)
        for body in ({}, {'id':1.5}, {'id':-1}, {'id':'1'}, {'id':1e100}, {'id':2147483648}):
            self.request('/api/stop','POST',body,400)
        self.request('/api/note','POST',{'id':1234,'note':'x'},404)
        self.request('/api/stop','POST',[],400)

    def test_task_node_todo_progress_persistence_and_cascade(self):
        task=self.request('/api/tasks','POST',{'title':"研究 O'Brien",'description':'目标\n下一步','status':'todo'},201)['id']
        node=self.request(f'/api/tasks/{task}/nodes','POST',{'title':'准确率偏低','description':'记录测试条件','status':'open'},201)['id']
        todo=self.request(f'/api/nodes/{node}/todos','POST',{'title':'检查替代梯度'},201)['id']
        entry=self.request(f'/api/todos/{todo}/progress','POST',{'content':"已排查 bias\n下一步测试 tau's value"},201)['id']
        self.request(f'/api/todos/{todo}','PUT',{'title':'检查梯度与状态','done':True})
        self.request(f'/api/nodes/{node}','PUT',{'title':'准确率偏低','description':'已定位原因','status':'resolved'})
        self.request(f'/api/tasks/{task}','PUT',{'title':'复现实验','description':'完成','status':'done'})
        self.stop(); self.start()
        board=self.request('/api/tasks')
        self.assertEqual(board['tasks'][0]['status'],'done')
        self.assertEqual(board['nodes'][0]['status'],'resolved')
        self.assertTrue(board['todos'][0]['done'])
        self.assertIn("tau's",board['progress'][0]['content'])
        self.assertEqual(board['progress'][0]['todo_id'],todo)
        self.assertEqual(board['progress'][0]['node_id'],node)
        self.request(f'/api/progress/{entry}','DELETE')
        self.assertEqual(self.request('/api/tasks')['progress'],[])
        self.request(f'/api/todos/{todo}/progress','POST',{'content':'第二条日志'},201)
        self.request(f'/api/tasks/{task}','DELETE')
        self.assertEqual(self.request('/api/tasks'),dict(tasks=[],nodes=[],todos=[],progress=[]))
        self.assertEqual(len(self.request('/api/history')),1)

    def test_board_validation_and_missing_parents(self):
        self.request('/api/tasks','POST',{'title':'  ','description':'','status':'todo'},400)
        self.request('/api/tasks','POST',{'title':'x','description':'','status':'invalid'},400)
        self.request('/api/tasks/999/nodes','POST',{'title':'x','description':'','status':'open'},404)
        self.request('/api/nodes/999/todos','POST',{'title':'x'},404)
        self.request('/api/nodes/999/progress','POST',{'content':'x'},410)
        self.request('/api/todos/999/progress','POST',{'content':'x'},404)
        self.request('/api/todos/999/progress','POST',{'content':'  '},400)
        self.request('/api/tasks/999','PUT',{'title':'x','description':'','status':'todo'},404)
        self.request('/api/nodes/0','PUT',{'title':'x','description':'','status':'open'},400)
        self.request('/api/todos/0','PUT',{'title':'x','done':True},400)
        self.request('/api/todos/1','PUT',{'title':'x','done':'false'},400)
        self.request('/api/tasks/999','DELETE',status=404)
        self.request('/api/sessions/1','DELETE',status=404)

    def test_todo_progress_isolation_and_cascade(self):
        task=self.request('/api/tasks','POST',{'title':'隔离测试','description':'','status':'todo'},201)['id']
        node=self.request(f'/api/tasks/{task}/nodes','POST',{'title':'问题','description':'','status':'open'},201)['id']
        todos=[self.request(f'/api/nodes/{node}/todos','POST',{'title':title},201)['id'] for title in ('排查','验证')]
        for todo in todos:
            self.request(f'/api/todos/{todo}/progress','POST',{'content':f'待办 {todo} 的日志'},201)
        self.assertEqual({p['todo_id'] for p in self.request('/api/tasks')['progress']},set(todos))
        self.request(f'/api/todos/{todos[0]}','DELETE')
        remaining=self.request('/api/tasks')['progress']
        self.assertEqual(len(remaining),1)
        self.assertEqual(remaining[0]['todo_id'],todos[1])
        self.request(f'/api/nodes/{node}','DELETE')
        board=self.request('/api/tasks')
        self.assertEqual(board['progress'],[])
        self.assertEqual(board['todos'],[])
        self.assertEqual(len(board['tasks']),1)

    def test_legacy_progress_migration_is_lossless_and_runs_once(self):
        self.stop()
        self.reset_legacy_board(0)
        legacy=[(7,10,"旧记录 O'Brien\n下一步",1700000000),(9,10,'第二次排查',1700000100),(12,20,'另一节点',1700000200)]
        with sqlite3.connect(self.root/'test.db') as db:
            db.execute("INSERT INTO tasks VALUES(1,'原任务','','doing',100,100)")
            db.executemany("INSERT INTO problem_nodes VALUES(?,1,'原节点','','open',100,100)",[(10,),(20,),(30,)])
            db.execute("INSERT INTO node_todos VALUES(5,10,'原有已完成待办',1)")
            db.executemany('INSERT INTO node_progress VALUES(?,?,?,?)',legacy)
        self.start()
        board=self.request('/api/tasks')
        historical=[t for t in board['todos'] if t['id']!=5]
        self.assertEqual(len(historical),2)
        self.assertEqual({t['node_id'] for t in historical},{10,20})
        self.assertTrue(next(t for t in board['todos'] if t['id']==5)['done'])
        self.assertEqual(sorted((p['id'],p['node_id'],p['content'],p['created_at']) for p in board['progress']),legacy)
        ownership={t['id']:t['node_id'] for t in historical}
        self.assertTrue(all(ownership[p['todo_id']]==p['node_id'] for p in board['progress']))
        self.stop(); self.start()
        self.assertEqual(self.request('/api/tasks'),board)
        for todo in historical: self.request(f"/api/todos/{todo['id']}",'DELETE')
        self.stop(); self.start()
        self.assertEqual(self.request('/api/tasks')['progress'],[])
        self.assertEqual([t['id'] for t in self.request('/api/tasks')['todos']],[5])
        new=self.request('/api/todos/5/progress','POST',{'content':'升级后的新日志'},201)
        self.assertGreater(new['id'],12)

    def test_note_locations_roundtrip_and_older_client_updates(self):
        location="个人电脑 D:\\Notes\\O'Brien 实验.md\n共享盘：项目A/第3册，第25页"
        task_body={'title':'任务','description':'简介','status':'doing','note_location':location}
        task=self.request('/api/tasks','POST',task_body,201)['id']
        node_body={'title':'问题','description':'说明','status':'open','note_location':location}
        node=self.request(f'/api/tasks/{task}/nodes','POST',node_body,201)['id']
        todo_body={'title':'行动','note_location':location}
        todo=self.request(f'/api/nodes/{node}/todos','POST',todo_body,201)['id']
        for group in ('tasks','nodes','todos'):
            self.assertEqual(self.request('/api/tasks')[group][0]['note_location'],location)
        # Older clients and checkbox/status shortcuts omit the new field.
        del task_body['note_location']; del node_body['note_location']
        self.request(f'/api/tasks/{task}','PUT',task_body)
        self.request(f'/api/nodes/{node}','PUT',node_body)
        self.request(f'/api/todos/{todo}','PUT',{'title':'行动','done':True})
        self.stop(); self.start()
        for group in ('tasks','nodes','todos'):
            self.assertEqual(self.request('/api/tasks')[group][0]['note_location'],location)
        self.request(f'/api/todos/{todo}','PUT',{'title':'行动','done':True,'note_location':''})
        self.assertEqual(self.request('/api/tasks')['todos'][0]['note_location'],'')
        self.assertEqual(self.request('/api/tasks')['nodes'][0]['note_location'],location)
        for route,body in ((f'/api/tasks/{task}',task_body),(f'/api/nodes/{node}',node_body),(f'/api/todos/{todo}',{'title':'行动','done':False})):
            for invalid in (None,123,{},'中'*1334):
                self.request(route,'PUT',{**body,'note_location':invalid},400)
        default=self.request(f'/api/nodes/{node}/todos','POST',{'title':'无位置'},201)['id']
        self.assertEqual(next(t for t in self.request('/api/tasks')['todos'] if t['id']==default)['note_location'],'')

    def test_v1_migration_preserves_board_and_is_idempotent(self):
        self.stop(); self.reset_legacy_board(1)
        with sqlite3.connect(self.root/'test.db') as db:
            db.execute("INSERT INTO tasks VALUES(1,'旧任务','旧说明','doing',100,120)")
            db.execute("INSERT INTO problem_nodes VALUES(2,1,'旧节点','','resolved',110,130)")
            db.execute("INSERT INTO node_todos VALUES(3,2,'已完成行动',1)")
            db.execute("INSERT INTO todo_progress VALUES(4,3,'仍需保留的日志',140)")
        self.start()
        board=self.request('/api/tasks')
        for group in ('tasks','nodes','todos'):
            self.assertEqual(board[group][0]['note_location'],'')
        self.assertTrue(board['todos'][0]['done'])
        self.assertEqual(board['progress'][0]['content'],'仍需保留的日志')
        self.assertEqual(board['progress'][0]['created_at'],140)
        self.assertEqual(board['tasks'][0]['updated_at'],120)
        with sqlite3.connect(self.root/'test.db') as db:
            self.assertEqual(db.execute('PRAGMA user_version').fetchone()[0],2)
        self.stop(); self.start()
        self.assertEqual(self.request('/api/tasks'),board)
        self.assertEqual(len(self.request('/api/history')),1)

if __name__=='__main__': unittest.main(verbosity=2)
