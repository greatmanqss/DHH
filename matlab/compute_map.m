function [map] = compute_map(dis_mtx, query_label, database_label)
tic;

q_num = length(query_label);%��ѯ�Ĵ���
d_num = length(database_label);%���ݿ�Ĺ�ģ
map = zeros(q_num, 1);%ÿһ����ѯ��Ӧһ��AP

database_label_mtx = repmat(database_label, 1, q_num);%ÿ�����ݿ��е�������label
sorted_database_label_mtx = database_label_mtx;%���ڼ�¼ÿ�����������ÿ��������label

[mtx idx_mtx] = sort(dis_mtx, 1);%��ÿ�μ����Ľ���������򣨰��н��У�

for q = 1 : q_num
    sorted_database_label_mtx(:, q) = database_label_mtx(idx_mtx(:, q), q);%label��������
end

result_mtx = (sorted_database_label_mtx == repmat(query_label', d_num, 1));%����������ÿ��λ�õ������Ƿ�Ͳ�ѯ��ǩһ�£�0/1

for q = 1 : q_num
    Qi = sum(result_mtx(:, q));%���������һ���ж��ٸ������Ͳ�ѯ��صĽ��
    map(q) = sum( ([1:Qi]') ./ (find(result_mtx(:, q) == 1)) ) / Qi;%�����������ѯ��ص�λ�õ�Precision��Ȼ���ۼ���ͣ��������������õ�AP
end

map = mean(map);
toc;
end